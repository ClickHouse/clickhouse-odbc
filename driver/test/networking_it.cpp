#include <charconv>
#include <cstdlib>
#include <span>
#include <random>
#include <asio.hpp>
#include <gtest/gtest.h>
#include <zstd.h>
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
                    "Transfer-Encoding: chunked\r\n"

#define ZSTD_HEADER "Content-Encoding: zstd\r\n"

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

        auto connection_string = fromUTF8<PTChar>(
#if defined(UNICODE)
            "Driver={ClickHouse ODBC Driver (Unicode)};"
#else
            "Driver={ClickHouse ODBC Driver (ANSI)};"
#endif
            "URL=http://127.0.0.1:" + std::to_string(server.port()) + "/;Timeout=5");
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

    void executeInserts(bool close_cursor = false) {
        auto query = fromUTF8<PTChar>("INSERT INTO test VALUES (?)");
        ODBC_CALL_ON_STMT_THROW(stmt, SQLPrepare(stmt, ptcharCast(query.data()), SQL_NTS));
        for (SQLINTEGER value = 0; value < 5; ++value) {
            ODBC_CALL_ON_STMT_THROW(stmt, SQLBindParameter(
                stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &value, sizeof(value), nullptr));
            ODBC_CALL_ON_STMT_THROW(stmt, SQLExecute(stmt));
            if (close_cursor)
                ODBC_CALL_ON_STMT_THROW(stmt, SQLFreeStmt(stmt, SQL_CLOSE));
        }
    }

    using KeepAlive = TcpServer::KeepAlive;

    TcpServer server{0};
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

// A reset connection is reported by the OS: "Connection reset by peer" (POSIX), "Connection was reset" (Windows).
#define EXPECT_THROW_RESET(expr) \
    EXPECT_THROW({ \
        try { \
            expr; \
        } catch (const std::runtime_error & ex) { \
            const std::string what = ex.what(); \
            ASSERT_TRUE(what.starts_with("1:[HY000][1]Recv failure: Connection") && what.find("reset") != std::string::npos) << what; \
            throw; \
        } \
    }, std::runtime_error);

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
        std::uniform_int_distribution<uint64_t> dist{0, INT32_MAX};
        size_t size = 0;
        while (size < min_size) {
            const auto value = dist(rnd_gen);
            const auto value_str = std::to_string(value);
            const uint32_t value_len = value_str.size();
            char value_len_bytes[4] = {};
            memcpy(value_len_bytes, &value_len, sizeof(value_len_bytes));
            stream.write(value_len_bytes, sizeof(value_len_bytes));
            stream << std::to_string(value);
            sum += value;
            size += value_len + sizeof(value_len_bytes);
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

    ClickHouseResponseGenerator & compress(ZSTD_EndDirective end_op = ZSTD_e_end)
    {
        std::unique_ptr<ZSTD_CStream, decltype(&ZSTD_freeCStream)> zstream{ZSTD_createCStream(), ZSTD_freeCStream};
        ZSTD_initCStream(zstream.get(), 1);

        auto input = stream.str();
        std::vector<char> output(ZSTD_compressBound(input.size()), '\0');
        size_t input_pos = 0;

        ZSTD_inBuffer in = {input.data(), input.size(), 0};
        ZSTD_outBuffer out = {output.data(), output.size(), 0};

        auto res = ZSTD_compressStream2(zstream.get(), &out, &in, end_op);
        if (res > 0 )
            throw std::runtime_error("insufficient buffer size");

        stream = std::stringstream{};
        stream.write(output.data(), out.pos);

        return *this;
    }

    std::vector<char> make_response(std::string_view headers = "")
    {
        std::string res = HTTP_HEADER + std::string(headers) + "\r\n" + stream.str();
        return std::vector<char>(res.begin(), res.end());
    }

    uint64_t expected_sum()
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
    uint64_t sum{0};
    std::mt19937 rnd_gen{42};
};

} // anonymous namespace

uint64_t fetch_sum(SQLHSTMT stmt)
{
    auto query = fromUTF8<PTChar>("SELECT 1");
    ODBC_CALL_ON_STMT_THROW(stmt, SQLExecDirect(stmt, ptcharCast(query.data()), SQL_NTS));

    int32_t number{};
    SQLLEN number_indicator{};
    ODBC_CALL_ON_STMT_THROW(stmt, SQLBindCol(stmt, 1, SQL_C_LONG, &number, 0, &number_indicator));

    uint64_t total = 0;
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
    for (int i = 0; i < 3; ++i)
        ASSERT_EQ(fetch_sum(stmt), gen.expected_sum());
    EXPECT_EQ(server.connectionCount(), 1);
}

TEST_F(NetworkingTest, RepeatedInsertsReuseChunkedConnection)
{
    const std::string response = HTTP_HEADER CRLF ZERO_CHUNK;
    setResponse(KeepAlive::KeepAlive, {response.begin(), response.end()});
    executeInserts();
    EXPECT_EQ(server.connectionCount(), 1);
}

TEST_F(NetworkingTest, RepeatedInsertsReuseContentLengthConnection)
{
    const std::string response = "HTTP/1.1 200 OK\r\nConnection: Keep-Alive\r\nContent-Length: 0\r\n\r\n";
    setResponse(KeepAlive::KeepAlive, {response.begin(), response.end()});
    executeInserts();
    EXPECT_EQ(server.connectionCount(), 1);
}

TEST_F(NetworkingTest, RepeatedInsertsReuseCompressedConnection)
{
    ClickHouseResponseGenerator gen{};
    gen.cut(0).compress().chunk(128).append_last_chunk();
    setResponse(KeepAlive::KeepAlive, gen.make_response(ZSTD_HEADER));
    executeInserts();
    EXPECT_EQ(server.connectionCount(), 1);
}

TEST_F(NetworkingTest, ClosingInsertCursorPreservesConnection)
{
    const std::string response = HTTP_HEADER CRLF ZERO_CHUNK;
    setResponse(KeepAlive::KeepAlive, {response.begin(), response.end()});
    executeInserts(true);
    EXPECT_EQ(server.connectionCount(), 1);
}

TEST_F(NetworkingTest, RepeatedInsertsReconnectWhenServerCloses)
{
    const std::string response = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
    setResponse(KeepAlive::Close, {response.begin(), response.end()});
    executeInserts();
    EXPECT_EQ(server.connectionCount(), 5);
}

TEST_F(NetworkingTest, ClosingUnreadCursorReconnects)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 512).chunk(1024).append_last_chunk();
    setResponse(KeepAlive::KeepAlive, gen.make_response());

    auto query = fromUTF8<PTChar>("SELECT 1");
    ODBC_CALL_ON_STMT_THROW(stmt, SQLExecDirect(stmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(stmt, SQLFreeStmt(stmt, SQL_CLOSE));
    ASSERT_EQ(fetch_sum(stmt), gen.expected_sum());
    EXPECT_EQ(server.connectionCount(), 2);
}

TEST_F(NetworkingTest, ClosingFullyReceivedCursorReusesConnection)
{
    // This entire response is received before the cursor is closed, so the connection
    // stays usable even though the application did not read all rows.
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).append_last_chunk();
    setResponse(KeepAlive::KeepAlive, gen.make_response());

    auto query = fromUTF8<PTChar>("SELECT 1");
    ODBC_CALL_ON_STMT_THROW(stmt, SQLExecDirect(stmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(stmt, SQLFreeStmt(stmt, SQL_CLOSE));
    ASSERT_EQ(fetch_sum(stmt), gen.expected_sum());
    EXPECT_EQ(server.connectionCount(), 1);
}

TEST_F(NetworkingTest, IncompleteResponseReconnects)
{
    const std::string incomplete_response = HTTP_HEADER CRLF "1\r\n";
    setResponse(KeepAlive::Close, {incomplete_response.begin(), incomplete_response.end()});

    auto query = fromUTF8<PTChar>("INSERT INTO test SELECT 1");
    ODBC_CALL_ON_STMT_THROW(stmt, SQLPrepare(stmt, ptcharCast(query.data()), SQL_NTS));
    EXPECT_THROW_MESSAGE(
        ODBC_CALL_ON_STMT_THROW(stmt, SQLExecute(stmt)),
        std::runtime_error, "1:[HY000][1]transfer closed with outstanding read data remaining");

    const std::string response = HTTP_HEADER CRLF ZERO_CHUNK;
    setResponse(KeepAlive::KeepAlive, {response.begin(), response.end()});
    ODBC_CALL_ON_STMT_THROW(stmt, SQLExecute(stmt));
    ODBC_CALL_ON_STMT_THROW(stmt, SQLExecute(stmt));
    EXPECT_EQ(server.connectionCount(), 2);
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
       const std::string what = ex.what();
       ASSERT_TRUE(what.starts_with("1:[HY000][1]Recv failure: Connection")) << what;
    }
}

/**
 * Verifies that the library throws a "connection reset" exception when the server
 * drops the connection in the middle of an incomplete chunk (1024 bytes declared, only 384
 * bytes sent).
 */
TEST_F(NetworkingTest, ConnectionDropMidStream)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).cut(1024 * 4 - 10);

    setResponse(KeepAlive::Drop, gen.make_response());
    EXPECT_THROW_RESET(fetch_sum(stmt));
}
/*
 * Verifies that the library throws a "connection reset" exception when the server
 * drops the connection after sending complete chunks but without the required zero-length
 * terminating chunk.
 */
TEST_F(NetworkingTest, ConnectionDropMissingZeroChunk)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).cut(1024 * 4 - 10);
    setResponse(KeepAlive::Drop, gen.make_response());
    EXPECT_THROW_RESET(fetch_sum(stmt));
}

/**
 * Verifies that the library throws an "outstanding read data remaining" exception when
 * the server properly closes the connection mid-stream during an incomplete chunk.
 */
TEST_F(NetworkingTest, ConnectionCloseMidStream)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024).cut(1024 * 4 - 10);

    setResponse(KeepAlive::Close, gen.make_response());
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, "1:[HY000][1]transfer closed with outstanding read data remaining");
}

/**
 * Verifies that the library throws an "outstanding read data remaining" exception when
 * the server properly closes the connection after complete chunks but without sending
 * the required zero-length terminating chunk.
 */
TEST_F(NetworkingTest, ConnectionCloseMissingZeroChunk)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 5).chunk(1024);

    setResponse(KeepAlive::Close, gen.make_response());
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, "1:[HY000][1]transfer closed with outstanding read data remaining");
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
        "1:[HY000][1]chunk hex-length char not a hex digit: 0xa");
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

TEST_F(NetworkingTest, PositiveCaseKeepAliveCompressed)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 512).compress().chunk(128).append_last_chunk();
    setResponse(KeepAlive::KeepAlive, gen.make_response(ZSTD_HEADER));
    for (int i = 0; i < 3; ++i)
        ASSERT_EQ(fetch_sum(stmt), gen.expected_sum());
    EXPECT_EQ(server.connectionCount(), 1);
}

TEST_F(NetworkingTest, ClickHouseExceptionCompressed)
{
    ClickHouseResponseGenerator gen{};

    // NOTE: it only works with ZSTD_e_flush
    gen.generate(1024 * 512).append(CH_EXCEPTION_TEXT).compress(ZSTD_e_flush).chunk(128);

    setResponse(KeepAlive::Close, gen.make_response(ZSTD_HEADER));
    EXPECT_THROW_MESSAGE(
        fetch_sum(stmt),
        std::runtime_error,
        "1:[HY000][1]ClickHouse exception: Code: 395. DB::Exception: "
        "ClickHouse Exception. (CLICKHOUSE_EXCEPTION) (version 25.5.8.1)");
}

TEST_F(NetworkingTest, InterruptedZstdStream)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 512).compress();
    auto out_size = gen.make_response().size();
    gen.cut(out_size - 512).chunk(256).append_last_chunk();

    setResponse(KeepAlive::KeepAlive, gen.make_response(ZSTD_HEADER));
    EXPECT_THROW_MESSAGE(
        fetch_sum(stmt),
        std::runtime_error,
        "1:[HY000][1]Failed to decompress the data: "
        "Incomplete data, frame was truncated or connection closed prematurely");
}

namespace {

std::string taggedException(const std::string & tag, const std::string & message)
{
    return "\r\n__exception__\r\n" + tag + "\r\n" + message + std::to_string(message.size()) + " " + tag + "\r\n__exception__\r\n";
}

const std::string TAGGED_MESSAGE
    = "Code: 395. DB::Exception: Tagged. (FUNCTION_THROW_IF_VALUE_IS_NON_ZERO) (version 26.9.7.9)\n";

} // anonymous namespace

/**
 * Verifies the handling of a ClickHouse mid-stream exception in the format with an exception tag
 * (X-ClickHouse-Exception-Tag).
 */
TEST_F(NetworkingTest, ClickHouseExceptionTagged)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 512).append(taggedException("abcdefgh", TAGGED_MESSAGE)).chunk(1000);
    setResponse(KeepAlive::Close, gen.make_response("X-ClickHouse-Exception-Tag: abcdefgh\r\n"));
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error,
        ("1:[HY000][1]ClickHouse exception: " + TAGGED_MESSAGE).c_str());
}

TEST_F(NetworkingTest, ClickHouseExceptionTaggedCompressed)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 1024 * 4).append(taggedException("abcdefgh", TAGGED_MESSAGE)).compress(ZSTD_e_flush).chunk(4096);
    setResponse(KeepAlive::Close, gen.make_response(ZSTD_HEADER "X-ClickHouse-Exception-Tag: abcdefgh\r\n"));
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error,
        ("1:[HY000][1]ClickHouse exception: " + TAGGED_MESSAGE).c_str());
}

/**
 * The server resets the connection (RST) right after the exception. On Linux the data received
 * before the reset is still delivered, so the ClickHouse exception must be reported. Other systems
 * (e.g. Windows) may discard unread data when a reset arrives, so a reset error is acceptable there.
 */
TEST_F(NetworkingTest, ClickHouseExceptionTaggedDrop)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 64).append(taggedException("abcdefgh", TAGGED_MESSAGE)).chunk(1000);
    setResponse(KeepAlive::Drop, gen.make_response("X-ClickHouse-Exception-Tag: abcdefgh\r\n"));
    try {
        fetch_sum(stmt);
        FAIL() << "expected an exception";
    } catch (const std::runtime_error & ex) {
        const std::string what = ex.what();
#if defined(__linux__)
        EXPECT_EQ(what, "1:[HY000][1]ClickHouse exception: " + TAGGED_MESSAGE);
#else
        EXPECT_TRUE(what == "1:[HY000][1]ClickHouse exception: " + TAGGED_MESSAGE
            || what.starts_with("1:[HY000][1]Recv failure: Connection")) << what;
#endif
    }
}

TEST_F(NetworkingTest, LargeResponseKeepAlive)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 1024 * 4).chunk(65536).append_last_chunk();
    setResponse(KeepAlive::KeepAlive, gen.make_response());
    for (int i = 0; i < 2; ++i)
        ASSERT_EQ(fetch_sum(stmt), gen.expected_sum());
    EXPECT_EQ(server.connectionCount(), 1);
}

/**
 * A compressed response framed with Content-Length instead of the chunked encoding.
 */
TEST_F(NetworkingTest, ContentLengthCompressedResponse)
{
    ClickHouseResponseGenerator gen{};
    gen.generate(1024 * 64).compress();
    const auto with_headers = gen.make_response();
    const std::string body = std::string(with_headers.begin(), with_headers.end()).substr(std::string(HTTP_HEADER CRLF).size());
    const std::string response = "HTTP/1.1 200 OK\r\nConnection: Keep-Alive\r\n" ZSTD_HEADER
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    setResponse(KeepAlive::KeepAlive, {response.begin(), response.end()});
    ASSERT_EQ(fetch_sum(stmt), gen.expected_sum());
    ASSERT_EQ(fetch_sum(stmt), gen.expected_sum());
    EXPECT_EQ(server.connectionCount(), 1);
}

TEST_F(NetworkingTest, ErrorStatusWithBody)
{
    const std::string body = "Code: 60. DB::Exception: Unknown table. (UNKNOWN_TABLE)\n";
    const std::string response = "HTTP/1.1 404 Not Found\r\nX-ClickHouse-Exception-Code: 60\r\nContent-Length: "
        + std::to_string(body.size()) + "\r\n\r\n" + body;
    setResponse(KeepAlive::KeepAlive, {response.begin(), response.end()});
    const auto expected = "1:[HY000][1]HTTP status code: 404\nReceived error:\n" + body + "\n";
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, expected.c_str());
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error, expected.c_str());
    EXPECT_EQ(server.connectionCount(), 1);
}

TEST_F(NetworkingTest, RedirectLimit)
{
    const std::string response = "HTTP/1.1 307 Temporary Redirect\r\nLocation: /elsewhere?x=1\r\nContent-Length: 0\r\n\r\n";
    setResponse(KeepAlive::KeepAlive, {response.begin(), response.end()});
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error,
        "1:[HY000][1]Redirect count exceeded\nRedirect limit: 10\n");
}

TEST_F(NetworkingTest, RedirectWithoutLocation)
{
    const std::string response = "HTTP/1.1 307 Temporary Redirect\r\nContent-Length: 0\r\n\r\n";
    setResponse(KeepAlive::KeepAlive, {response.begin(), response.end()});
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error,
        "1:[HY000][1]HTTP status code: 307, but no Location header in the response");
}

/**
 * Redirects to another scheme are rejected, so that e.g. https -> http cannot expose the credentials.
 * (The test server speaks plain HTTP only, so http -> https is tested; it is the same check.)
 */
TEST_F(NetworkingTest, RedirectSchemeChangeRejected)
{
    const auto target = "https://127.0.0.1:" + std::to_string(server.port()) + "/x";
    const std::string response = "HTTP/1.1 307 Temporary Redirect\r\nLocation: " + target + "\r\nContent-Length: 0\r\n\r\n";
    setResponse(KeepAlive::KeepAlive, {response.begin(), response.end()});
    EXPECT_THROW_MESSAGE(fetch_sum(stmt), std::runtime_error,
        ("1:[HY000][1]Redirect to a different scheme is not allowed: " + target).c_str());
}
