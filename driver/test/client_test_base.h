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

    void connectWithDSN(const std::string & dsn)
    {
        auto encoded_dsn = fromUTF8<PTChar>(dsn);
        ODBC_CALL_ON_DBC_THROW(hdbc, SQLConnect(hdbc, ptcharCast(encoded_dsn.data()), SQL_NTS, NULL, 0, NULL, 0));
        ODBC_CALL_ON_DBC_THROW(hdbc, SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt));
    }

    void connect(const std::string & connection_string) {
        auto encoded_cs = fromUTF8<PTChar>(connection_string);
        ODBC_CALL_ON_DBC_THROW(hdbc, SQLDriverConnect(hdbc, NULL, ptcharCast(encoded_cs.data()), SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT));
        ODBC_CALL_ON_DBC_THROW(hdbc, SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt));
    }

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

protected:
    virtual void SetUp() override {
        ODBC_CALL_ON_ENV_THROW(henv, SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv));
        ODBC_CALL_ON_ENV_THROW(henv, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0));

        ODBC_CALL_ON_ENV_THROW(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));

        if (!skip_connect_)
            connectWithDSN(TestEnvironment::getInstance().getDSN());
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
