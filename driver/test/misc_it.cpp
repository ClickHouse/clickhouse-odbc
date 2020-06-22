#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>

class MiscellaneousTest
    : public ClientTestBase
{
};

TEST_F(MiscellaneousTest, RowArraySizeAttribute) {
    SQLRETURN rc = SQL_SUCCESS;
    SQLULEN size = 0;

    {
        size = 123;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 1);
    }
    
    {
        size = 0;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)size, 0));
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        size = 123;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 0);
    }

    {
        size = 1;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)size, 0));
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        size = 123;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 1);
    }

    {
        size = 456;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)size, 0));
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        size = 0;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 456);
    }
}
