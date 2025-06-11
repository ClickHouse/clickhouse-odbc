#pragma once

#include "driver/platform/platform.h"
#include "driver/test/gtest_env.h"
#include "driver/test/client_utils.h"

#include <gtest/gtest.h>

template <typename Base>
class ClientTestBaseMixin
    : public Base
{
public:
    ClientTestBaseMixin(bool skip_connect = false)
        : skip_connect_(skip_connect)
    {
    }

    virtual ~ClientTestBaseMixin() {
        if (hstmt) {
            SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
            hstmt = nullptr;
        }

        if (hdbc) {
            SQLDisconnect(hdbc);
            SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
            hdbc = nullptr;
        }

        if (henv) {
            SQLFreeHandle(SQL_HANDLE_ENV, henv);
            henv = nullptr;
        }
    }

protected:
    virtual void SetUp() override {
        ODBC_CALL_ON_ENV_THROW(henv, SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv));
        ODBC_CALL_ON_ENV_THROW(henv, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0));

        ODBC_CALL_ON_ENV_THROW(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));

        if (!skip_connect_) {
            auto dsn = fromUTF8<PTChar>(TestEnvironment::getInstance().getDSN());
            auto * dsn_wptr =dsn.data();

            ODBC_CALL_ON_DBC_THROW(hdbc, SQLConnect(hdbc, ptcharCast(dsn_wptr), SQL_NTS, NULL, 0, NULL, 0));
            ODBC_CALL_ON_DBC_THROW(hdbc, SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt));
        }
    }

    virtual void TearDown() override {
        if (hstmt) {
            ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeHandle(SQL_HANDLE_STMT, hstmt));
            hstmt = nullptr;
        }

        if (hdbc) {
            /*ODBC_CALL_ON_DBC_LOG(hdbc, */SQLDisconnect(hdbc)/*)*/;
            ODBC_CALL_ON_DBC_THROW(hdbc, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));
            hdbc = nullptr;
        }

        if (henv) {
            ODBC_CALL_ON_ENV_THROW(henv, SQLFreeHandle(SQL_HANDLE_ENV, henv));
            henv = nullptr;
        }
    }

protected:
    const bool skip_connect_;
    SQLHENV henv = nullptr;
    SQLHDBC hdbc = nullptr;
    SQLHSTMT hstmt = nullptr;
};

using ClientTestBase = ClientTestBaseMixin<::testing::Test>;

template <typename Params>
using ClientTestWithParamBase = ClientTestBaseMixin<::testing::TestWithParam<Params>>;
