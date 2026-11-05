#include "driver/connection.h"
#include "driver/environment.h"

#include <gtest/gtest.h>

// These tests only check how the connection string is parsed into the Connection configuration fields.
// Because `Driver=` is specified, no DSN lookup is performed, and because `VerifyConnectionEarly` is off
// by default, no request is sent to the server, so no running ClickHouse instance is required.
class ConnectionConfigurationTest
    : public ::testing::Test
{
protected:
    Environment environment{Driver::getInstance()};
    Connection connection{environment};
};

TEST_F(ConnectionConfigurationTest, AccessTokenIsEmptyByDefault) {
    connection.connect("Driver=x");
    EXPECT_TRUE(connection.access_token.empty());
}

TEST_F(ConnectionConfigurationTest, AccessTokenIsSaved) {
    connection.connect("Driver=x;AccessToken=abc");
    EXPECT_EQ(connection.access_token, "abc");
}

TEST_F(ConnectionConfigurationTest, AccessTokenKeyIsCaseInsensitive) {
    connection.connect("Driver=x;accesstoken=abc");
    EXPECT_EQ(connection.access_token, "abc");

    connection.connect("Driver=x;ACCESSTOKEN=def");
    EXPECT_EQ(connection.access_token, "def");
}

TEST_F(ConnectionConfigurationTest, AccessTokenJwtIsSavedAsIs) {
    const std::string jwt =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"
        ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ"
        ".SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c-_==";

    connection.connect("Driver=x;AccessToken=" + jwt);
    EXPECT_EQ(connection.access_token, jwt);

    connection.connect("Driver=x;AccessToken={" + jwt + "}");
    EXPECT_EQ(connection.access_token, jwt);
}

TEST_F(ConnectionConfigurationTest, AccessTokenIsResetOnReconnect) {
    connection.connect("Driver=x;AccessToken=abc");
    EXPECT_EQ(connection.access_token, "abc");

    connection.connect("Driver=x");
    EXPECT_TRUE(connection.access_token.empty());
}
