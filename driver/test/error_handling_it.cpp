#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"
#include "driver/test/result_set_reader.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

class ErrorHandlingTest
    : public ClientTestBase
{
public:
    std::string getQueryId()
    {
        char query_id_data[74] = {};
        SQLINTEGER query_id_len{};
        SQLGetStmtAttr(hstmt, SQL_CH_STMT_ATTR_LAST_QUERY_ID, query_id_data, std::size(query_id_data), &query_id_len);

#ifdef _win_
        return toUTF8(reinterpret_cast<PTChar*>(query_id_data));
#else
        // unixODBC usually converts all strings from the driver encoding to the application encoding,
        // similar to how Windows behaves. However, `SQLGetStmtAttr` appears to be an exception in unixODBC,
        // as it passes the result in the driver encoding unchanged.
        // See: https://github.com/lurcher/unixODBC/issues/22
        //
        // To solve this issue and ensure the result reaches the application in the correct encoding,
        // we would need to implement both `SQLGetStmtAttr` and `SQLGetStmtAttrW` in the driver.
        // This would introduce a number of workarounds and non-uniform solutions.
        // Currently, there is only one string attribute, `SQL_CH_STMT_ATTR_LAST_QUERY_ID`, which is internal,
        // so this added complexity in the driver is not currently justified.
        //
        // Since we know that the Query ID is 36 bytes long, we can infer the encoding of the result
        // and re-encode it accordingly here in the test.
        if (query_id_len == 36)
            return toUTF8(query_id_data);
        else
            return toUTF8(reinterpret_cast<char16_t*>(query_id_data));
#endif
    }
};

/**
 * Given a query with a syntax error, we receive the error description with the query id.
 */
TEST_F(ErrorHandlingTest, ServerExceptionInBeginning)
{
    auto query = fromUTF8<PTChar>("SELECT * FROM this.table.cannot.exist");

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ASSERT_EQ(SQLExecute(hstmt), SQL_ERROR);
    auto error_message = extract_diagnostics(hstmt, SQL_HANDLE_STMT);
    auto query_id = getQueryId();

    auto expect_error_message = "1:[HY000][1]Error while processing query " + query_id + ": HTTP status code: 400";
    ASSERT_EQ(error_message.substr(0, expect_error_message.size()), expect_error_message);
}

/**
 * Given a query with an exception in the middle of the data stream, we receive the error description with the query id.
 * (We do not yet parse error messages, we only report "Incomplete input stream, expected at least" at the moment)
 */
TEST_F(ErrorHandlingTest, ServerExceptionInMiddleOfStream)
{
    static const size_t stream_size = 100000;
    auto query = fromUTF8<PTChar>(
        "SELECT throwIf(number >= "
        + std::to_string(stream_size) + ") FROM numbers("
        + std::to_string(stream_size) + " + 1)");

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    std::string error_message = "";
    SQLRETURN res = SQL_SUCCESS;
    while (res == SQL_SUCCESS)
    {
        res = SQLFetch(hstmt);
        if (res != SQL_SUCCESS)
            error_message = extract_diagnostics(hstmt, SQL_HANDLE_STMT);
    }

    auto query_id = getQueryId();
    auto expect_error_message = "1:[HY000][1]Error while processing query " + query_id + ": ";
    ASSERT_EQ(error_message.substr(0, expect_error_message.size()), expect_error_message);
}
