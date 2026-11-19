#include "odbc.h"
#include "runner.h"

#include <gtest/gtest.h>

using namespace runner;

TEST(Runner, ParseUseDatabase) {
    const std::map<std::string, std::string> params = {{"CLICKHOUSE_DATABASE", "test_x"}};
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"USE db1;", "db1"},
        {"use `my db`", "my db"},
        {"-- c\nUSE DATABASE db2", "db2"},
        {"USE {CLICKHOUSE_DATABASE:Identifier};", "test_x"},
        {"USE \"db3\"", "db3"},
    };
    for (const auto & [in, want] : cases) {
        std::string got;
        EXPECT_TRUE(parseUseDatabase(in, params, got)) << in;
        EXPECT_EQ(want, got) << in;
    }
    std::string got;
    EXPECT_FALSE(parseUseDatabase("USE db1, db2", params, got));
}

TEST(Runner, ParseSetAssignments) {
    const std::vector<std::pair<std::string, std::string>> want = {{"dialect", "kusto"}, {"max_threads", "1"}, {"x", "(1, 2)"}};
    EXPECT_EQ(want, parseSetAssignments("SET dialect = 'kusto', max_threads=1, x = (1, 2);"));
}

TEST(Runner, ConnectionString) {
    Options opts;
    opts.host = "127.0.0.1";
    opts.port = 9000;
    opts.database = "test_abc";
    opts.settings = {{"log_comment", "00001_select_1.sql-test_abc"}, {"send_logs_level", "warning"}};
    opts.params = {{"x", "a b;c"}};
    InvocationLog log;
    ::unsetenv("CLICKHOUSE_PORT_TCP");
    ::unsetenv("CLICKHOUSE_PORT_HTTP");
    Runner r(opts, Backend::ODBCDriver2, "/x/libclickhouseodbc.so", log, stdout);
    const auto url = r.url();
    const std::string prefix = "http://127.0.0.1:8123/?session_id=clickhouse-test-runner-";
    const std::string suffix = "&log_comment=00001_select_1.sql-test_abc&send_logs_level=warning&param_x=a%20b%3Bc";
    ASSERT_EQ(prefix.size() + 32 + suffix.size(), url.size()) << url;
    EXPECT_EQ(prefix, url.substr(0, prefix.size()));
    EXPECT_EQ(suffix, url.substr(url.size() - suffix.size()));
    EXPECT_EQ("Driver=/x/libclickhouseodbc.so;Url=" + url + ";UID=default;PWD=;Database=test_abc;Timeout=300", r.connectionString());

    // Each invocation has its own session.
    Runner other(opts, Backend::ODBCDriver2, "/x/libclickhouseodbc.so", log, stdout);
    EXPECT_NE(url, other.url());

    Runner rb(opts, Backend::RowBinary, "/x/libclickhouseodbc.so", log, stdout);
    EXPECT_NE(std::string::npos, rb.url().find("&default_format=RowBinaryWithNamesAndTypes&log_comment="));
}

TEST(Runner, ParseServerCode) {
    std::string text;
    const auto code = parseServerCode("[HY000] Error while processing query 9c69: HTTP status code: 404\nReceived error:\n"
                                      "Code: 60. DB::Exception: Unknown table. (UNKNOWN_TABLE)\n\n",
        text);
    ASSERT_TRUE(code.has_value());
    EXPECT_EQ(60, *code);
    EXPECT_EQ("Code: 60. DB::Exception: Unknown table. (UNKNOWN_TABLE)", text);
    EXPECT_FALSE(parseServerCode("[HY000] 'TSV' format is not supported", text).has_value());
    EXPECT_EQ(395, parseServerCode("ClickHouse exception: Code: 395. DB::Exception: x", text));
}

TEST(Runner, ParseServerCodeOfOtherExceptions) {
    // The server's text for an exception that is not a DB::Exception (src/Common/Exception.cpp).
    std::string text;
    EXPECT_EQ(1000,
        parseServerCode("[HY000] Error while processing query 1: HTTP status code: 500\nReceived error:\n"
                        "Poco::Exception. Code: 1000, e.code() = 0, Bad URI syntax: URI contains invalid characters\n",
            text));
    EXPECT_EQ("Code: 1000, e.code() = 0, Bad URI syntax: URI contains invalid characters", text);
    EXPECT_EQ(1001, parseServerCode("std::exception. Code: 1001, type: std::runtime_error, e.what() = ran out of bytes", text));
    EXPECT_EQ(1002, parseServerCode("Unknown exception. Code: 1002, type: int", text));
    // Only `Code: N.` and `Code: N, ` are codes.
    EXPECT_FALSE(parseServerCode("[HY000] HTTP status code: 500", text).has_value());
    EXPECT_FALSE(parseServerCode("Code: 1000,x", text).has_value());
    EXPECT_FALSE(parseServerCode("Code: 7", text).has_value());
}

TEST(Runner, KustoLetJoinsTheNextQuery) {
    EXPECT_TRUE(isKustoLet("kusto", "let threshold = 15;"));
    EXPECT_TRUE(isKustoLet("Kusto", "-- c\nlet t = datatable (N:long) [1];"));
    EXPECT_FALSE(isKustoLet("kusto", "datatable (N:long) [1] | where N > threshold;"));
    EXPECT_FALSE(isKustoLet("", "let x = 1;"));
    EXPECT_FALSE(isKustoLet("clickhouse", "let x = 1;"));
    EXPECT_FALSE(isKustoLet("prql", "let x = (from t);"));
    EXPECT_TRUE(isKustoLet("kusto", "// -2, a signed literal\n  // more\nlet s = (a:timespan = 1h) { a };"));
    EXPECT_FALSE(isKustoLet("kusto", "// let x = 1\nprint 1;"));

    const auto st = splitStatements("let a = 1; -- first\nlet b = 2;\nprint a + b; -- { serverError 62 }\n");
    ASSERT_EQ(3U, st.size());
    const auto joined = joinStatements(joinStatements(st[0], st[1]), st[2]);
    EXPECT_EQ("let a = 1; -- first\nlet b = 2;\nprint a + b;", joined.sql);
    EXPECT_EQ("let a = 1; -- first\nlet b = 2;\nprint a + b; -- { serverError 62 }", joined.full);
    EXPECT_TRUE(joined.hint.expectsError());
    EXPECT_TRUE(joined.hint.matches(62));
}

TEST(Runner, KustoBraceDepth) {
    EXPECT_EQ(0, kustoBraceDepth("let Sq = (x:long) { x * x };"));
    EXPECT_EQ(1, kustoBraceDepth("let UsesOuter = (x:long) { let y = x + Outer;"));
    EXPECT_EQ(-1, kustoBraceDepth(" y * 2 };"));
    EXPECT_EQ(0, kustoBraceDepth("print '{', \"{\" // {\n;"));

    const auto st = splitStatements("let F = (x:long) { let y = x + 1; y * 2 };\nprint F(1);\n");
    ASSERT_EQ(3U, st.size());
    const auto body = joinStatements(st[0], st[1]);
    EXPECT_EQ(0, kustoBraceDepth(body.sql));
    EXPECT_TRUE(isKustoLet("kusto", body.sql));
}

TEST(Runner, StatementSyntaxError) {
    EXPECT_TRUE(isStatementSyntaxError(62,
        "Code: 62. DB::Exception: Syntax error: failed at position 11 (end of query): . "
        "Expected one of: token. (SYNTAX_ERROR)"));
    // The statement parsed; formatQuerySingleLine could not parse its argument.
    EXPECT_FALSE(isStatementSyntaxError(62,
        "Code: 62. DB::Exception: Syntax error: failed at position 87 (end of query): . "
        "Expected one of: token, Comma, end of query: In scope SELECT "
        "formatQuerySingleLine('BACKUP TABLE t'). (SYNTAX_ERROR)"));
    EXPECT_FALSE(isStatementSyntaxError(60, "Code: 60. DB::Exception: Unknown table. (UNKNOWN_TABLE)"));
    EXPECT_FALSE(isStatementSyntaxError(std::nullopt, ""));
}
