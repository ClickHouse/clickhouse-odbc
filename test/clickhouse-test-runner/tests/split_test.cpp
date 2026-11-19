#include "split.h"
#include "text.h"

#include <gtest/gtest.h>

using namespace runner;

namespace {

std::vector<std::string> sqls(const std::vector<Statement> & stmts) {
    std::vector<std::string> out;
    for (const auto & s : stmts)
        out.push_back(s.sql);
    return out;
}

struct SplitCase {
    const char * name;
    std::string in;
    std::vector<std::string> want;
};

} // namespace

TEST(Split, Statements) {
    const std::vector<SplitCase> cases = {
        {"single without semicolon", "SELECT 1", {"SELECT 1"}},
        {"several", "SELECT 1;\nSELECT 2;\n", {"SELECT 1;", "SELECT 2;"}},
        {"semicolon in string", "SELECT 'a;b'; SELECT 2", {"SELECT 'a;b';", "SELECT 2"}},
        {"escaped quote", R"(SELECT 'a\';b'; SELECT 2)", {R"(SELECT 'a\';b';)", "SELECT 2"}},
        {"doubled quote", "SELECT 'a'';b'; SELECT 2", {"SELECT 'a'';b';", "SELECT 2"}},
        {"semicolon in identifier", "SELECT 1 AS `a;b`; SELECT 2", {"SELECT 1 AS `a;b`;", "SELECT 2"}},
        {"semicolon in line comment", "SELECT 1 -- a;b\n; SELECT 2", {"SELECT 1 -- a;b\n;", "SELECT 2"}},
        {"semicolon in block comment", "SELECT /* a;b */ 1; SELECT 2", {"SELECT /* a;b */ 1;", "SELECT 2"}},
        {"nested block comment", "SELECT /* a /* b; */ c; */ 1; SELECT 2", {"SELECT /* a /* b; */ c; */ 1;", "SELECT 2"}},
        {"heredoc", "SELECT $$a;'b$$; SELECT $x$;$x$", {"SELECT $$a;'b$$;", "SELECT $x$;$x$"}},
        {"trailing comment only", "SELECT 1;\n-- the end\n", {"SELECT 1;"}},
        {"empty", "  \n-- only a comment\n", {}},
        {"tags header is skipped", "-- Tags: no-fasttest\n-- Tag no-fasttest: reason\n\nSELECT 1;", {"SELECT 1;"}},
        {"leading comment stays with its statement", "SELECT 1;\n-- next\nSELECT 2;", {"SELECT 1;", "-- next\nSELECT 2;"}},
        {"insert data ends at an empty line",
            "INSERT INTO t FORMAT CSV\n1,'a;b'\n2,c;\n\nSELECT 1;",
            {"INSERT INTO t FORMAT CSV\n1,'a;b'\n2,c;", "SELECT 1;"}},
        {"insert values ends at a semicolon",
            "INSERT INTO t FORMAT Values (1);\nSELECT 1;",
            {"INSERT INTO t FORMAT Values (1);", "SELECT 1;"}},
        {"format of a select is not insert data", "SELECT 1 FORMAT TSV;\nSELECT 2;", {"SELECT 1 FORMAT TSV;", "SELECT 2;"}},
        {"insert select with a column named format",
            "INSERT INTO t SELECT format FROM s;\nSELECT 1;",
            {"INSERT INTO t SELECT format FROM s;", "SELECT 1;"}},
        {"insert select from input has inline data",
            "INSERT INTO t SELECT x FROM input('x UInt8') FORMAT TSV\n1;\n2\n\nSELECT 1;",
            {"INSERT INTO t SELECT x FROM input('x UInt8') FORMAT TSV\n1;\n2", "SELECT 1;"}},
        {"insert select with a format alias",
            "INSERT INTO t SELECT 1 AS format FROM numbers(1);\nSELECT 1;",
            {"INSERT INTO t SELECT 1 AS format FROM numbers(1);", "SELECT 1;"}},
        {"insert select with an implicit alias after format",
            "INSERT INTO t SELECT format x FROM s;\nSELECT 1;",
            {"INSERT INTO t SELECT format x FROM s;", "SELECT 1;"}},
        {"insert select with format DIV",
            "INSERT INTO t SELECT format DIV 2 FROM s;\nSELECT 1;",
            {"INSERT INTO t SELECT format DIV 2 FROM s;", "SELECT 1;"}},
        {"insert select with format GLOBAL IN after FROM",
            "INSERT INTO t SELECT x FROM s WHERE format GLOBAL IN (1);\nSELECT 1;",
            {"INSERT INTO t SELECT x FROM s WHERE format GLOBAL IN (1);", "SELECT 1;"}},
        {"insert with format before the data",
            "INSERT INTO t (a) SETTINGS async_insert = 0 FORMAT CSV\n1\n\nSELECT 1;",
            {"INSERT INTO t (a) SETTINGS async_insert = 0 FORMAT CSV\n1", "SELECT 1;"}},
        {"double semicolon", "SELECT 1;;\nSELECT 2", {"SELECT 1;", "SELECT 2"}},
    };
    for (const auto & c : cases)
        EXPECT_EQ(c.want, sqls(splitStatements(c.in))) << c.name;
}

TEST(Split, TrailingHint) {
    const auto stmts = splitStatements("SELECT throwIf(1); -- { serverError FUNCTION_THROW_IF_VALUE_IS_NON_ZERO }\nSELECT 2;");
    ASSERT_EQ(2u, stmts.size());
    EXPECT_EQ("SELECT throwIf(1); -- { serverError FUNCTION_THROW_IF_VALUE_IS_NON_ZERO }", stmts[0].full);
    EXPECT_EQ(std::vector<int> {395}, stmts[0].hint.server_errors);
    EXPECT_FALSE(stmts[1].hint.expectsError());
}

TEST(Split, HintOnNextLineIsLeading) {
    // clickhouse-client takes only the same-line comment as the trailing comment. A hint on the
    // next line is a leading comment of the next statement, and an error hint in a leading
    // comment is ignored.
    const auto stmts = splitStatements("SELECT 1;\n-- { serverError 60 }\nSELECT 2;");
    ASSERT_EQ(2u, stmts.size());
    EXPECT_FALSE(stmts[0].hint.expectsError());
    EXPECT_FALSE(stmts[1].hint.expectsError());
}

TEST(Split, HintInsideStatement) {
    const auto stmts = splitStatements("SELECT x -- { serverError UNKNOWN_IDENTIFIER }\n;");
    ASSERT_EQ(1u, stmts.size());
    EXPECT_EQ(std::vector<int> {47}, stmts[0].hint.server_errors);
}

TEST(Split, Echo) {
    const auto stmts
        = splitStatements("SELECT 1;\n-- { echoOn }\nSELECT 2; -- two\nSELECT 3;\n-- { echoOff }\nSELECT 4;\nSELECT 5; -- { echo }\n");
    ASSERT_EQ(5u, stmts.size());
    std::vector<bool> echo;
    for (const auto & s : stmts)
        echo.push_back(s.hint.echo_this);
    EXPECT_EQ((std::vector<bool> {false, true, true, false, true}), echo);
    EXPECT_EQ("-- { echoOn }\nSELECT 2; -- two", stmts[1].full);
}

TEST(Split, LeadingKeyword) {
    EXPECT_EQ("SELECT", leadingKeyword("  -- c\n/* d */ (select 1)"));
    EXPECT_EQ("INSERT", leadingKeyword("insert into t values (1)"));
    EXPECT_EQ("", leadingKeyword("-- only a comment"));
}

TEST(Split, KeepsSemicolon) {
    // clickhouse-client sends the semicolon; system.query_log shows it.
    const auto stmts = splitStatements("SELECT 1; -- c\nSELECT 2\n");
    ASSERT_EQ(2u, stmts.size());
    EXPECT_EQ("SELECT 1;", stmts[0].sql);
    EXPECT_EQ("SELECT 2", stmts[1].sql);
}

TEST(Split, EchoOfLastStatementWithoutSemicolon) {
    // clickhouse-client echoes the rest of the text, with the newlines.
    const auto stmts = splitStatements("SELECT 1;\nSELECT 2\n\n");
    ASSERT_EQ(2u, stmts.size());
    EXPECT_EQ("SELECT 1;", stmts[0].full);
    EXPECT_EQ("SELECT 2\n\n", stmts[1].full);
    EXPECT_EQ("SELECT 2", stmts[1].sql);
}
