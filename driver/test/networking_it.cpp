#include <cstdlib>
#include <span>
#include <asio.hpp>
#include <gtest/gtest.h>
#include "driver/utils/conversion.h"
#include "driver/test/client_utils.h"

#if defined(_WIN32)
#    define NOMINMAX
#    define WIN32_LEAN_AND_MEAN
#    define VC_EXTRALEAN
#    include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>

#include "http_server/TestHTTPServer.h"

namespace {

#define ODBC_HEADER "30\r\n"                                           \
    "\x02\x00\x00\x00\x02\x00\x00\x00\x04\x00\x00\x00\x6E\x61\x6D\x65" \
    "\x06\x00\x00\x00\x6E\x75\x6D\x62\x65\x72\x02\x00\x00\x00\x04\x00" \
    "\x00\x00\x74\x79\x70\x65\x06\x00\x00\x00\x55\x49\x6E\x74\x36\x34" \
    "\r\n"

#define B128 \
    "\x02\x00\x00\x00\x38\x31\x02\x00\x00\x00\x38\x32\x02\x00\x00\x00" \
    "\x38\x33\x02\x00\x00\x00\x38\x34\x02\x00\x00\x00\x38\x35\x02\x00" \
    "\x00\x00\x38\x36\x02\x00\x00\x00\x38\x37\x02\x00\x00\x00\x38\x38" \
    "\x02\x00\x00\x00\x38\x39\x02\x00\x00\x00\x39\x30\x02\x00\x00\x00" \
    "\x39\x31\x02\x00\x00\x00\x39\x32\x02\x00\x00\x00\x39\x33\x02\x00" \
    "\x00\x00\x39\x34\x02\x00\x00\x00\x39\x35\x02\x00\x00\x00\x39\x36" \
    "\x02\x00\x00\x00\x39\x37\x02\x00\x00\x00\x39\x38\x02\x00\x00\x00" \
    "\x39\x39\x03\x00\x00\x00\x31\x30\x30\x03\x00\x00\x00\x31\x30\x31"

#define SINGLE_VALUE "\x02\x00\x00\x00\x38"

#define CH_EXCEPTION \
    "__exception__\r\n" \
    "Code: 395. DB::Exception: ClickHouse Exception. (CLICKHOUSE_EXCEPTION) (version 25.5.8.1)"

#define CRLF "\r\n"

// 1 Kbyte of chunked encoded data the size (0x400 = 1024)
#define KB_SIZE "400\r\n"

// chunk of just one Kbyte of data
#define KB KB_SIZE B128 B128 B128 B128 B128 B128 B128 B128 CRLF

#define KB_DATA_SUM 15288

#define HTTP_HEADER "HTTP/1.1 200 OK\r\n"                          \
                    "Connection: Keep-Alive\r\n"                   \
                    "Content-Type: application/octet-stream\r\n"   \
                    "Transfer-Encoding: chunked\r\n\r\n"

#define ZERO_CHUNK "0" CRLF CRLF

class NetworkingTest : public testing::Test
{
public:
    NetworkingTest() = default;

protected:

    virtual void SetUp() override {
        ODBC_CALL_ON_ENV_THROW(env, SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env));
        ASSERT_TRUE(env);

        ODBC_CALL_ON_ENV_THROW(env, SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
        ODBC_CALL_ON_ENV_THROW(env, SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc));
        ASSERT_TRUE(dbc);

        SQLTCHAR final_connection_string[1024];
        SQLSMALLINT final_connection_string_len = 0;

        auto connection_string
            = fromUTF8<PTChar>("Driver={ClickHouse ODBC Driver (Unicode)};URL=http://127.0.0.1:8124/");
        ODBC_CALL_ON_DBC_THROW(dbc, SQLDriverConnect(
            dbc,
            nullptr,
            ptcharCast(connection_string.data()),
            SQL_NTS,
            final_connection_string, sizeof(final_connection_string),
            &final_connection_string_len, SQL_DRIVER_NOPROMPT));

        ODBC_CALL_ON_DBC_THROW(dbc, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt));
        ASSERT_TRUE(stmt);
    }

    virtual void TearDown() override {
        ODBC_CALL_ON_STMT_THROW(stmt, SQLFreeStmt(stmt, SQL_CLOSE));
        ODBC_CALL_ON_STMT_THROW(stmt, SQLFreeHandle(SQL_HANDLE_STMT, stmt));
        ODBC_CALL_ON_DBC_THROW(dbc, SQLDisconnect(dbc));
        ODBC_CALL_ON_DBC_THROW(dbc, SQLFreeHandle(SQL_HANDLE_DBC, dbc));
        ODBC_CALL_ON_DBC_THROW(dbc, SQLFreeHandle(SQL_HANDLE_ENV, env));
    }

    template <size_t Size>
    void setResponse(TcpServer::KeepAlive keep_alive, const char (&response)[Size]) {
        server.setResponse(std::vector<char>(std::begin(response), std::end(response) - 1));
        server.setKeepAlive(keep_alive);
    }

    using KeepAlive = TcpServer::KeepAlive;

    TcpServer server{8124};
    SQLHENV env{nullptr};
    SQLHDBC dbc{nullptr};
    SQLHSTMT stmt{nullptr};
};

#define EXPECT_THROW_MESSAGE(expr, type, message) \
    EXPECT_THROW({ \
        try { \
            expr; \
        } catch (const type & ex) { \
            ASSERT_STREQ(ex.what(), (message)); \
            throw; \
        } \
    }, type);

} // anonymous namespace

int64_t fetch_sum(SQLHSTMT stmt)
{
    auto query = fromUTF8<PTChar>("SELECT 1");
    ODBC_CALL_ON_STMT_THROW(stmt, SQLExecDirect(stmt, ptcharCast(query.data()), SQL_NTS));

    int32_t number{};
    SQLLEN number_indicator{};
    ODBC_CALL_ON_STMT_THROW(stmt, SQLBindCol(stmt, 1, SQL_C_LONG, &number, 0, &number_indicator));

    int64_t total = 0;
    while (true) {
        SQLRETURN res = SQLFetch(stmt);
        if (res == SQL_NO_DATA)
            break;

        ODBC_CALL_ON_STMT_THROW(stmt, res);

        if (number_indicator != SQL_NULL_DATA)
            total += number;
    }

    return total;
}

/**
 * Verifies that the driver correctly processes 5KB of chunked transfer-encoded data when
 * the server maintains a persistent connection (Keep-Alive). This is most common positive
 * case, if it doesn not work - nothing works.
 */
TEST_F(NetworkingTest, PositiveCaseKeepAlive)
{
    setResponse(KeepAlive::KeepAlive, HTTP_HEADER ODBC_HEADER KB KB KB KB KB ZERO_CHUNK);
    ASSERT_EQ(fetch_sum(stmt), KB_DATA_SUM * 5);
}

/**
 * Verifies that the library correctly processes 5KB of chunked transfer-encoded data when
 * the server closes the connection after the response. Less common but again normal
 * case that must always work.
 */
TEST_F(NetworkingTest, PositiveCaseKeepClose)
{
    setResponse(KeepAlive::Close, HTTP_HEADER ODBC_HEADER KB KB KB KB KB ZERO_CHUNK);
    ASSERT_EQ(fetch_sum(stmt), KB_DATA_SUM * 5);
}

/**
 * Verifies that the library handles either successful processing of 5KB of chunked data
 * or gracefully catches a connection reset exception when the server abruptly drops
 * the connection without proper closure (RST instead of FIN is sent by the server).
 * DISABLED: On Windows Poco returns no data at all in this case, in linux it works
 * as expected.
 */
TEST_F(NetworkingTest, DISABLED_PositiveCaseDrop)
{
    // This produces an error on Windows, however this does not seem to be critical.
    // Either way, getting a correct sum or having an exception seem fine in this case
    setResponse(KeepAlive::Drop, HTTP_HEADER ODBC_HEADER KB KB KB KB KB ZERO_CHUNK);
    int64_t sum = 0;
    try {
        sum = fetch_sum(stmt);
        ASSERT_EQ(sum, KB_DATA_SUM * 5);
    } catch (const std::runtime_error & ex) {
       ASSERT_STREQ(ex.what(), "1:[HY000][1]Connection reset by peer");
    }
}

/**
 * Verifies that the library throws a "Connection reset by peer" exception when the server
 * drops the connection in the middle of an incomplete chunk (1024 bytes declared, only 384
 * bytes sent).
 * DISABLED: Poco exception is not handled, the driver returns nothing.
 */
TEST_F(NetworkingTest, DISABLED_ConnectionDropMidStream)
{
    setResponse(KeepAlive::Drop, HTTP_HEADER ODBC_HEADER KB KB KB KB KB "400\r\n" B128 B128 B128);
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, "1:[HY000][1]Connection reset by peer");
}
/*
 * Verifies that the library throws a "Connection reset by peer" exception when the server
 * drops the connection after sending complete chunks but without the required zero-length
 * terminating chunk.
 * DISABLED: Poco exception is not handled, the driver returns nothing.
 */
TEST_F(NetworkingTest, DISABLED_ConnectionDropMissingZeroChunk)
{
    setResponse(KeepAlive::Drop, HTTP_HEADER ODBC_HEADER KB KB KB KB KB);
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, "1:[HY000][1]Connection reset by peer");
}

/**
 * Verifies that the library throws an "Unexpected EOF in chunked encoding" exception when
 * the server properly closes the connection mid-stream during an incomplete chunk.
 * DISABLED: Poco does not report any exceptions in this case and closes the stream as nothing
 * bad has happened.
 */
TEST_F(NetworkingTest, DISABLED_ConnectionCloseMidStream)
{
    setResponse(KeepAlive::Close, HTTP_HEADER ODBC_HEADER KB KB KB KB KB "400\r\n" B128 B128 B128);
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, "1:[HY000][1]Unexpected EOF in chunked encoding");
}

/**
 * Verifies that the library throws an "Unexpected EOF in chunked encoding" exception when
 * the server properly closes the connection after complete chunks but without sending
 * the required zero-length terminating chunk.
 * DISABLED: Poco does not report any exceptions in this case and closes the stream as nothing
 * bad has happened.
 */
TEST_F(NetworkingTest, DISABLED_ConnectionCloseMissingZeroChunk)
{
    setResponse(KeepAlive::Close, HTTP_HEADER ODBC_HEADER KB KB KB KB KB);
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, "1:[HY000][1]Unexpected EOF in chunked encoding");
}

/**
 * Verifies that the library throws an exception when it receives data that violates chunked
 * transfer encoding format (raw data without proper chunk size headers).
 * DISABLED: Poco does not report any exceptions in this case and closes the stream as nothing
 * bad has happened.
 */
TEST_F(NetworkingTest, DISABLED_IncorrectEncoding)
{
    setResponse(KeepAlive::KeepAlive, HTTP_HEADER ODBC_HEADER KB KB KB KB KB B128 B128 B128);
    EXPECT_THROW(fetch_sum(stmt), std::runtime_error);
}

/**
 * Verifies the library's handling of a ClickHouse server exception that arrives aligned
 * with data boundaries, i.e. all data before the exception message can be parsed correctly.
 * DISABLED: The driver does not process such exceptions at the moment
 */
TEST_F(NetworkingTest, DISABLED_ClickHouseExceptionAligned)
{
    setResponse(KeepAlive::Drop, HTTP_HEADER ODBC_HEADER KB KB KB KB KB CH_EXCEPTION);
    EXPECT_THROW_MESSAGE(
        fetch_sum(stmt),
        std::runtime_error,
        "1:[HY000][1]Code: 395. DB::Exception: ClickHouse Exception. (CLICKHOUSE_EXCEPTION) (version 25.5.8.1)");
}

/**
 * Verifies the library's handling of a ClickHouse server exception that arrives unaligned
 * with data boundaries, i.e. all there is only a part of the record in the payload -
 * the last record cannot be parsed.
 * DISABLED: The driver does not process such exceptions at the moment
 */
TEST_F(NetworkingTest, DISABLED_ClickHouseExceptionUnaligned)
{
    const char response[] = HTTP_HEADER ODBC_HEADER KB KB KB KB KB "10" CRLF SINGLE_VALUE SINGLE_VALUE CH_EXCEPTION;
    setResponse(KeepAlive::Drop, response);
    EXPECT_THROW_MESSAGE(
        fetch_sum(stmt),
        std::runtime_error,
        "1:[HY000][1]Code: 395. DB::Exception: ClickHouse Exception. (CLICKHOUSE_EXCEPTION) (version 25.5.8.1)");
}
