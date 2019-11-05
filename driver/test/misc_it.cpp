#include "driver/platform/platform.h"
#include "driver/test/gtest_env.h"
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
        size = 0;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 1);
    }
    
    {
        size = 1;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)size, 0));
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        size = 0;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 1);
    }

    {
        size = 1234;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)size, 0));
        ASSERT_EQ(rc, SQL_SUCCESS_WITH_INFO); // TODO: remove this when row arrays bigger than 1 are allowed.
    }

    {
        size = 0;
        rc = ODBC_CALL_ON_DBC_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 1); // TODO: remove this when row arrays bigger than 1 are allowed.
//      ASSERT_EQ(size, 1234); // TODO: uncomment this, when row arrays bigger than 1 are allowed.
    }
}
