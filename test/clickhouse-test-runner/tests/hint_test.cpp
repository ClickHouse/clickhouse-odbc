#include "hint.h"

#include <gtest/gtest.h>

using namespace runner;

TEST(Hint, Parse) {
    struct Case {
        const char * name;
        const char * in;
        std::vector<int> server;
        std::vector<int> client;
    };
    const std::vector<Case> cases = {
        {"server error by name", "SELECT 1; -- { serverError UNKNOWN_TABLE }", {60}, {}},
        {"server error by code", "SELECT 1; -- { serverError 60 }", {60}, {}},
        {"several codes", "SELECT 1; -- { serverError 153, ILLEGAL_DIVISION }", {153, 153}, {}},
        {"client error", "SELECT; -- { clientError SYNTAX_ERROR }", {}, {62}},
        {"error sets both", "SELECT 1; -- { error 62 }", {62}, {62}},
        {"block comment", "SELECT 1 /* { serverError 60 } */;", {60}, {}},
        {"text after the hint", "SELECT 1; -- { serverError 60 } because there is no table", {60}, {}},
        {"leading hint is ignored", "-- { serverError 60 }\nSELECT 1;", {}, {}},
        {"braces in a string are not a hint", "SELECT '-- { serverError 60 }';", {}, {}},
        {"no hint", "SELECT 1; -- a comment", {}, {}},
    };
    for (const auto & c : cases) {
        const auto h = parseTestHint(c.in);
        EXPECT_EQ("", h.parse_error) << c.name;
        EXPECT_EQ(c.server, h.server_errors) << c.name;
        EXPECT_EQ(c.client, h.client_errors) << c.name;
    }
}

TEST(Hint, ParseErrors) {
    EXPECT_NE("", parseTestHint("SELECT 1; -- { serverError NOT_A_REAL_ERROR_NAME }").parse_error);
    EXPECT_NE("", parseTestHint("SELECT 1; -- { retry 3 until serverError 60 }").parse_error);
}

TEST(Hint, Matches) {
    const auto h = parseTestHint("SELECT; -- { clientError SYNTAX_ERROR }");
    // Through the driver the server finds syntax errors.
    EXPECT_TRUE(h.matches(62));
    EXPECT_FALSE(h.matches(60));
    EXPECT_EQ("clientError SYNTAX_ERROR", h.label());
}

TEST(Hint, Echo) {
    auto h = parseTestHint("-- { echoOn }\nSELECT 1;");
    ASSERT_TRUE(h.echo.has_value());
    EXPECT_TRUE(*h.echo);
    h = parseTestHint("SELECT 1; -- { echoOff }");
    ASSERT_TRUE(h.echo.has_value());
    EXPECT_FALSE(*h.echo);
}

TEST(Hint, ErrorCodes) {
    EXPECT_EQ(60, errorCodeByName("UNKNOWN_TABLE"));
    EXPECT_EQ("UNKNOWN_TABLE", errorNameByCode(60));
    EXPECT_FALSE(errorCodeByName("NOT_A_REAL_ERROR_NAME"));
}
