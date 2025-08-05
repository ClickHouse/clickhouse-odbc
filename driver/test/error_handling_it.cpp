#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"
#include "driver/test/result_set_reader.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

class ErrorHandlingTest
    : public ClientTestBase
{
public:
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
