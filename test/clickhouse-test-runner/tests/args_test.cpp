#include "args.h"

#include <gtest/gtest.h>

#include <cstdlib>

using namespace runner;

TEST(Args, SqlTestCommandLine) {
    // The command line that tests/clickhouse-test uses for a .sql test.
    const auto opts = parseArgs({"client",
        "--host=127.0.0.1",
        "--port=9000",
        "--no-secure",
        "--database=test_abc",
        "--send_logs_level=warning",
        "--multiquery",
        "--log_comment",
        "00001_select_1.sql-test_abc",
        "--max_block_size=1",
        "--allow_repeated_settings",
        "--client_logs_file=/tmp/x-fatal",
        "--param_CLICKHOUSE_DATABASE=test_abc"});
    EXPECT_EQ("127.0.0.1", opts.host);
    EXPECT_EQ(9000, opts.port);
    EXPECT_EQ("test_abc", opts.database);
    EXPECT_FALSE(opts.secure);
    EXPECT_FALSE(opts.query.has_value());
    const std::vector<std::pair<std::string, std::string>> settings = {
        {"send_logs_level", "warning"},
        {"log_comment", "00001_select_1.sql-test_abc"},
    };
    EXPECT_EQ(settings, opts.settings);
    EXPECT_EQ((std::map<std::string, std::string> {{"CLICKHOUSE_DATABASE", "test_abc"}}), opts.params);
}

TEST(Args, ShortOptions) {
    const auto opts = parseArgs({"-h", "db", "-u", "me", "-d", "x", "-q", "SELECT 1", "-n", "-s"});
    EXPECT_EQ("db", opts.host);
    EXPECT_EQ("me", opts.user);
    EXPECT_EQ("x", opts.database);
    ASSERT_TRUE(opts.query.has_value());
    EXPECT_EQ("SELECT 1", *opts.query);
    EXPECT_TRUE(opts.secure);
}

TEST(Args, SettingValues) {
    const auto opts = parseArgs({"--max_threads", "-1", "--max-insert-threads", "2", "--query=SELECT 1"});
    const std::vector<std::pair<std::string, std::string>> settings = {{"max_threads", "-1"}, {"max_insert_threads", "2"}};
    EXPECT_EQ(settings, opts.settings);
}

TEST(Args, RedactedCommandLine) {
    EXPECT_EQ("--host=h --password <redacted> --user u --password=<redacted> -q SELECT 1",
        redactedCommandLine({"--host=h", "--password", "p1", "--user", "u", "--password=p2", "-q", "SELECT 1"}));
    // The value of an option is the next argument, also when it looks like an option, as in parseArgs.
    EXPECT_EQ("--password <redacted> --port=9000", redactedCommandLine({"--password", "--password", "--port=9000"}));
    EXPECT_EQ("-q --password --password <redacted>", redactedCommandLine({"-q", "--password", "--password", "p"}));
    EXPECT_EQ("p", parseArgs({"-q", "--password", "--password", "p"}).password);
    // An empty password stays empty. After `--`, no argument is an option.
    EXPECT_EQ("--password= --password  --password", redactedCommandLine({"--password=", "--password", "", "--password"}));
    EXPECT_EQ("-- --password p", redactedCommandLine({"--", "--password", "p"}));
    EXPECT_EQ("", redactedCommandLine({}));
}

TEST(Args, Errors) {
    EXPECT_THROW(parseArgs({"--port=abc"}), std::invalid_argument);
    EXPECT_THROW(parseArgs({"--query"}), std::invalid_argument);
}

TEST(Args, HttpPort) {
    ::setenv("CLICKHOUSE_PORT_TCP", "", 1);
    ::setenv("CLICKHOUSE_PORT_HTTP", "", 1);
    EXPECT_EQ(8123, httpPort(0, false));
    EXPECT_EQ(8123, httpPort(9000, false));
    EXPECT_EQ(18123, httpPort(18123, false));
    EXPECT_EQ(8443, httpPort(9440, true));

    ::setenv("CLICKHOUSE_PORT_TCP", "19000", 1);
    ::setenv("CLICKHOUSE_PORT_HTTP", "18124", 1);
    EXPECT_EQ(18124, httpPort(19000, false));
    EXPECT_EQ(9000, httpPort(9000, false));
    ::unsetenv("CLICKHOUSE_PORT_TCP");
    ::unsetenv("CLICKHOUSE_PORT_HTTP");
}
