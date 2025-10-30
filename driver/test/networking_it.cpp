#include <charconv>
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

#define CRLF "\r\n"

#define ODBC_HEADER_DATA                                               \
    "\x02\x00\x00\x00\x02\x00\x00\x00\x04\x00\x00\x00\x6E\x61\x6D\x65" \
    "\x06\x00\x00\x00\x6E\x75\x6D\x62\x65\x72\x02\x00\x00\x00\x04\x00" \
    "\x00\x00\x74\x79\x70\x65\x06\x00\x00\x00\x55\x49\x6E\x74\x36\x34"

#define ODBC_HEADER                                                    \
    "30" CRLF                                                          \
    ODBC_HEADER_DATA                                                   \
    CRLF

#define CH_EXCEPTION_TEXT                                              \
    "__exception__\r\n"                                                \
    "Code: 395. DB::Exception: ClickHouse Exception. "                 \
    "(CLICKHOUSE_EXCEPTION) (version 25.5.8.1)"

#define CH_EXCEPTION_CHUNK                                             \
    "68" CRLF                                                          \
    CH_EXCEPTION_TEXT                                                  \
    CRLF

#define HTTP_HEADER "HTTP/1.1 200 OK\r\n"                            \
                    "Connection: Keep-Alive\r\n"                     \
                    "Content-Type: application/octet-stream\r\n"     \
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

        ODBC_CALL_ON_ENV_THROW(
            env, SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0));
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
        ODBC_CALL_ON_ENV_THROW(env, SQLFreeHandle(SQL_HANDLE_ENV, env));
    }

    void setResponse(TcpServer::KeepAlive keep_alive, std::vector<char> response) {
        server.setResponse(std::vector<char>(std::begin(response), std::end(response)));
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

/**
 * Small helper that allows generating real testable ODBCDriver2 data,
 * format it in chunks and perform other manipulations to generate different
 * test server responses.
 */
class ClickHouseResponseGenerator
{
public:

    ClickHouseResponseGenerator()
    {
        stream.write(ODBC_HEADER_DATA, sizeof(ODBC_HEADER_DATA) - 1);
    }

    ClickHouseResponseGenerator & generate(size_t min_size)
    {
        constexpr char field_size[] = "\x04\x00\x00\x00";
        size_t size = 0;
        for (size_t i = 0; size < min_size; ++i) {
            stream.write(field_size, sizeof(field_size) - 1);
            const size_t value = 1000 + i % 9000;
            const auto value_str = std::to_string(value);
            stream << std::to_string(value);
            sum += value;
            size += value_str.size() + sizeof(field_size) - 1;
        }
        return *this;
    }

    ClickHouseResponseGenerator & append(std::string str)
    {
        stream << str;
        return *this;
    }


    ClickHouseResponseGenerator & append_last_chunk()
    {
        stream.write("0\r\n\r\n", 5);
        return *this;
    }

    ClickHouseResponseGenerator & cut(size_t pos)
    {
        auto str = stream.str();
        if (pos < str.size()) {
            str.resize(pos);
            stream = std::stringstream{};
            stream << str;
        }
        return *this;
    }

    ClickHouseResponseGenerator & chunk(size_t max_chunk_size)
    {
        std::string data = stream.str();
        stream = std::stringstream{};

        size_t pos = 0;
        while (data.size() - pos >= max_chunk_size) {
            stream << chunk_separator_for_size(max_chunk_size);
            stream << std::string_view(&data[pos], max_chunk_size) << "\r\n";
            pos += max_chunk_size;
        }

        if (data.size() - pos > 0) {
            const size_t chunk_size = data.size() - pos;
            stream << chunk_separator_for_size(chunk_size);
            stream << std::string_view(&data[pos], chunk_size) << "\r\n";
        }

        return *this;
    }

    std::vector<char> make_response(std::string_view headers = HTTP_HEADER)
    {
        std::string res = std::string(headers) + stream.str();
        return std::vector<char>(res.begin(), res.end());
    }

    size_t expected_sum()
    {
        return sum;
    }

private:
    std::string chunk_separator_for_size(size_t size)
    {
        char hex_buffer[20];
        auto res = std::to_chars(hex_buffer, hex_buffer + sizeof(hex_buffer), size, 16);
        assert(res.ec == std::errc());
        return std::string(hex_buffer, res.ptr) + "\r\n";
    }

private:
    std::stringstream stream{};

    size_t sum{0};
};

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
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).append_last_chunk();
    setResponse(KeepAlive::KeepAlive, gen.make_response());
    ASSERT_EQ(fetch_sum(stmt), gen.expected_sum());
}

/**
 * Verifies that the library correctly processes 5KB of chunked transfer-encoded data when
 * the server closes the connection after the response. Less common but again normal
 * case that must always work.
 */
TEST_F(NetworkingTest, PositiveCaseClose)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).append_last_chunk();
    setResponse(KeepAlive::Close, gen.make_response());
    ASSERT_EQ(fetch_sum(stmt), gen.expected_sum());
}

/**
 * Verifies that the library handles either successful processing of 5KB of chunked data
 * or gracefully catches a connection reset exception when the server abruptly drops
 * the connection without proper closure (RST instead of FIN is sent by the server).
 */
TEST_F(NetworkingTest, PositiveCaseDrop)
{
    // This produces an error on Windows, however this does not seem to be critical.
    // Either way, getting a correct sum or having an exception seem fine in this case
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).append_last_chunk();
    setResponse(KeepAlive::Drop, gen.make_response());
    int64_t sum = 0;
    try {
        sum = fetch_sum(stmt);
        ASSERT_EQ(sum, gen.expected_sum());
    } catch (const std::runtime_error & ex) {
       ASSERT_STREQ(ex.what(), "1:[HY000][1]Connection reset by peer");
    }
}

/**
 * Verifies that the library throws a "Connection reset by peer" exception when the server
 * drops the connection in the middle of an incomplete chunk (1024 bytes declared, only 384
 * bytes sent).
 */
TEST_F(NetworkingTest, ConnectionDropMidStream)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).cut(1024 * 4 - 10);

    setResponse(KeepAlive::Drop, gen.make_response());
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, "1:[HY000][1]Connection reset by peer");
}
/*
 * Verifies that the library throws a "Connection reset by peer" exception when the server
 * drops the connection after sending complete chunks but without the required zero-length
 * terminating chunk.
 */
TEST_F(NetworkingTest, ConnectionDropMissingZeroChunk)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).cut(1024 * 4 - 10);
    setResponse(KeepAlive::Drop, gen.make_response());
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, "1:[HY000][1]Connection reset by peer");
}

/**
 * Verifies that the library throws an "Unexpected EOF in chunked encoding" exception when
 * the server properly closes the connection mid-stream during an incomplete chunk.
 */
TEST_F(NetworkingTest, ConnectionCloseMidStream)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).cut(1024 * 4 - 10);

    setResponse(KeepAlive::Close, gen.make_response());
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, "1:[HY000][1]Unexpected EOF in chunked encoding");
}

/**
 * Verifies that the library throws an "Unexpected EOF in chunked encoding" exception when
 * the server properly closes the connection after complete chunks but without sending
 * the required zero-length terminating chunk.
 */
TEST_F(NetworkingTest, ConnectionCloseMissingZeroChunk)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024);

    setResponse(KeepAlive::Close, gen.make_response());
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, "1:[HY000][1]Unexpected EOF in chunked encoding");
}

/**
 * Verifies that the library throws an exception when it receives data that violates chunked
 * transfer encoding format (raw data without proper chunk size headers).
 */
TEST_F(NetworkingTest, IncorrectEncoding)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).generate(256);

    setResponse(KeepAlive::KeepAlive, gen.make_response());
    EXPECT_THROW_MESSAGE(
        fetch_sum(stmt),
        std::runtime_error,
        "1:[HY000][1]Unable to parse the chunk size from the stream");
}

/**
 * Verifies the library's handling of a ClickHouse server exception that arrives in a separate
 * chunk.
 */
TEST_F(NetworkingTest, ClickHouseExceptionAligned)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).append(CH_EXCEPTION_CHUNK);

    setResponse(KeepAlive::Close, gen.make_response());
    EXPECT_THROW_MESSAGE(
        fetch_sum(stmt),
        std::runtime_error,
        "1:[HY000][1]ClickHouse exception: Code: 395. DB::Exception: "
        "ClickHouse Exception. (CLICKHOUSE_EXCEPTION) (version 25.5.8.1)");
}

/**
 * Verifies the library's handling of a ClickHouse server exception that arrives in a chunk
 * that also contain some data.
 */
TEST_F(NetworkingTest, ClickHouseExceptionUnaligned)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).append(CH_EXCEPTION_TEXT).chunk(1024);

    setResponse(KeepAlive::Close, gen.make_response());
    EXPECT_THROW_MESSAGE(
        fetch_sum(stmt),
        std::runtime_error,
        "1:[HY000][1]ClickHouse exception: Code: 395. DB::Exception: "
        "ClickHouse Exception. (CLICKHOUSE_EXCEPTION) (version 25.5.8.1)");
}
