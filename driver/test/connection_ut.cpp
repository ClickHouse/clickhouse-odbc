#include "driver/connection.h"
#include "driver/environment.h"
#include "driver/api/impl/impl.h"

#include <gtest/gtest.h>
#include <algorithm>
#include "driver/test/http_server/TestHTTPServer.h"

// These tests only check how the connection string is parsed into the Connection configuration fields.
// Because `Driver=` is specified, no DSN lookup is performed, and because `VerifyConnectionEarly` is off
// by default, no request is sent to the server, so no running ClickHouse instance is required.
class ConnectionConfigurationTest
    : public ::testing::Test
{
protected:
    // Connecting again requires disconnecting first, the way SQLDisconnect does.
    void reconnect(const std::string & connection_string) {
        connection.session.reset();
        connection.connect(connection_string);
    }

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

    reconnect("Driver=x;ACCESSTOKEN=def");
    EXPECT_EQ(connection.access_token, "def");
}

TEST_F(ConnectionConfigurationTest, AccessTokenJwtIsSavedAsIs) {
    const std::string jwt =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"
        ".eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ"
        ".SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c-_==";

    connection.connect("Driver=x;AccessToken=" + jwt);
    EXPECT_EQ(connection.access_token, jwt);

    reconnect("Driver=x;AccessToken={" + jwt + "}");
    EXPECT_EQ(connection.access_token, jwt);
}

TEST_F(ConnectionConfigurationTest, AccessTokenIsResetOnReconnect) {
    connection.connect("Driver=x;AccessToken=abc");
    EXPECT_EQ(connection.access_token, "abc");

    reconnect("Driver=x");
    EXPECT_TRUE(connection.access_token.empty());
}

TEST_F(ConnectionConfigurationTest, TimeoutAttributesAreIndependentUnsignedIntegers) {
    const auto handle = connection.getHandle();
    for (const auto attribute : {SQL_ATTR_CONNECTION_TIMEOUT, SQL_ATTR_LOGIN_TIMEOUT}) {
        for (const SQLUINTEGER value : {0u, 17u, 70000u, 0xffffffffu}) {
            ASSERT_EQ(impl::SetConnectAttr(handle, attribute,
                reinterpret_cast<SQLPOINTER>(static_cast<uintptr_t>(value)), 0), SQL_SUCCESS);
            SQLUINTEGER actual = 0xdeadbeef;
            SQLINTEGER length = 0;
            ASSERT_EQ(impl::GetConnectAttr(handle, attribute, &actual, sizeof(actual), &length), SQL_SUCCESS);
            EXPECT_EQ(actual, value);
            EXPECT_EQ(length, sizeof(SQLUINTEGER));
        }
    }
    EXPECT_EQ(connection.connection_timeout, 0xffffffffu);
    EXPECT_EQ(connection.timeout, 0xffffffffu);
}

TEST_F(ConnectionConfigurationTest, TimeoutOverridesSurviveConnectionAndReconnection) {
    const auto handle = connection.getHandle();
    ASSERT_EQ(impl::SetConnectAttr(handle, SQL_ATTR_CONNECTION_TIMEOUT,
        reinterpret_cast<SQLPOINTER>(7), 0), SQL_SUCCESS);
    ASSERT_EQ(impl::SetConnectAttr(handle, SQL_ATTR_LOGIN_TIMEOUT, nullptr, 0), SQL_SUCCESS);
    for (int i = 0; i < 2; ++i) {
        reconnect("Driver=x;Timeout=42");
        EXPECT_EQ(connection.connection_timeout, 7u);
        EXPECT_EQ(connection.timeout, 0u);
        EXPECT_EQ(connection.session->settings.connection_timeout, 7u);
        EXPECT_EQ(connection.session->settings.timeout, 0u);
    }
    ASSERT_EQ(impl::SetConnectAttr(handle, SQL_ATTR_CONNECTION_TIMEOUT, nullptr, 0), SQL_SUCCESS);
    ASSERT_EQ(impl::SetConnectAttr(handle, SQL_ATTR_LOGIN_TIMEOUT,
        reinterpret_cast<SQLPOINTER>(70000), 0), SQL_SUCCESS);
    EXPECT_EQ(connection.session->settings.connection_timeout, 0u);
    EXPECT_EQ(connection.session->settings.timeout, 70000u);
    reconnect("Driver=x;Timeout=10");
    EXPECT_EQ(connection.connection_timeout, 0u);
    EXPECT_EQ(connection.timeout, 70000u);
}

TEST_F(ConnectionConfigurationTest, TimeoutConfigurationStillAppliesWithoutOverrides) {
    connection.connect("Driver=x;Timeout=42");
    EXPECT_EQ(connection.connection_timeout, 42u);
    EXPECT_EQ(connection.timeout, 42u);
    reconnect("Driver=x");
    EXPECT_EQ(connection.connection_timeout, 30u);
    EXPECT_EQ(connection.timeout, 30u);
}

TEST(HttpTimeoutTest, DisabledInactivityTimeoutAllowsDelayedResponse) {
    TcpServer server(0);
    const std::string reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK";
    server.setResponse({reply.begin(), reply.end()});
    server.setResponseDelay(std::chrono::milliseconds(1500));
    HttpSettings settings;
    settings.connection_timeout = 0;
    settings.timeout = 0;
    auto session = std::make_shared<HttpSession>(settings);
    HttpResponse response(session, {"http://127.0.0.1:" + std::to_string(server.port()) + "/",
        std::make_shared<const std::string>("query"), {}});
    EXPECT_EQ(response.getStatus(), 200);
    EXPECT_EQ(response.readAll(100), "OK");
}

TEST(HttpTimeoutTest, FiniteInactivityTimeoutRejectsDelayedResponse) {
    TcpServer server(0);
    const std::string reply = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    server.setResponse({reply.begin(), reply.end()});
    server.setResponseDelay(std::chrono::milliseconds(2000));
    HttpSettings settings;
    settings.connection_timeout = 0;
    settings.timeout = 1;
    auto session = std::make_shared<HttpSession>(settings);
    EXPECT_THROW((HttpResponse(session, {"http://127.0.0.1:" + std::to_string(server.port()) + "/",
        std::make_shared<const std::string>("query"), {}})), std::runtime_error);
}

TEST(HttpTimeoutTest, ConnectionTimeoutUsesCurlRange) {
    HttpSettings settings;
    settings.connection_timeout = 0;
    EXPECT_EQ(settings.connectionTimeoutSeconds(), std::numeric_limits<long>::max());
    settings.connection_timeout = 7;
    EXPECT_EQ(settings.connectionTimeoutSeconds(), 7);
    settings.connection_timeout = std::numeric_limits<std::uint32_t>::max();
    EXPECT_EQ(settings.connectionTimeoutSeconds(), static_cast<long>(std::min<std::uint64_t>(
        std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<long>::max())));
}
