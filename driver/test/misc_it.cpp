#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

class MiscellaneousTest
    : public ClientTestBase
{
};

TEST_F(MiscellaneousTest, RowArraySizeAttribute) {
    SQLRETURN rc = SQL_SUCCESS;
    SQLULEN size = 0;

    {
        size = 123;
        rc = ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 1);
    }
    
    {
        size = 0;
        rc = ODBC_CALL_ON_STMT_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)size, 0));
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        size = 123;
        rc = ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 0);
    }

    {
        size = 1;
        rc = ODBC_CALL_ON_STMT_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)size, 0));
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        size = 123;
        rc = ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 1);
    }

    {
        size = 456;
        rc = ODBC_CALL_ON_STMT_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)size, 0));
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        size = 0;
        rc = ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 456);
    }
}

TEST_F(MiscellaneousTest, SQLGetData_ZeroOutputBufferSize) {
    const std::string col_str = "1234567890";
    const std::string query_orig = "SELECT CAST('" + col_str + "', 'String') AS col";

    const auto query = fromUTF8<SQLTCHAR>(query_orig);
    auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    SQLTCHAR col[100] = {};
    SQLLEN col_ind = 0;

    SQLRETURN rc = SQLFetch(hstmt);

    if (rc == SQL_ERROR)
        throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

    if (rc == SQL_SUCCESS_WITH_INFO)
        std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

    if (!SQL_SUCCEEDED(rc))
        throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

    rc = SQLGetData(
        hstmt,
        1,
        getCTypeFor<decltype(&col[0])>(),
        &col,
        0, // instead of sizeof(col)
        &col_ind
    );

    if (!SQL_SUCCEEDED(rc))
        throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

    ASSERT_EQ(rc, SQL_SUCCESS_WITH_INFO);

    const auto col_size_in_bytes = col_str.size() * sizeof(SQLTCHAR);
    ASSERT_EQ(col_ind, col_size_in_bytes); // SQLGetData returns size in bytes in col_ind, even when the output buffer size is set to 0.
    EXPECT_THAT(col, ::testing::Each(SQLTCHAR{}));

    ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
}
