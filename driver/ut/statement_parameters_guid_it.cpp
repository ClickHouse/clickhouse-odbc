#include "platform.h"
#include "gtest_env.h"
#include "client_utils.h"

#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <cstring>

class StatementParameterBinding
    : public ::testing::Test
{
protected:
    virtual void SetUp() override {
        ODBC_CALL_ON_ENV_THROW(henv, SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv));
        ODBC_CALL_ON_ENV_THROW(henv, SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0));

        ODBC_CALL_ON_ENV_THROW(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));
        ODBC_CALL_ON_DBC_THROW(hdbc, SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0));

        auto & dsn = TestEnvironment::getInstance().getDSN();

        ODBC_CALL_ON_DBC_THROW(hdbc, SQLConnect(hdbc, (SQLTCHAR*) dsn.c_str(), SQL_NTS, (SQLTCHAR*) NULL, 0, NULL, 0));
        ODBC_CALL_ON_DBC_THROW(hdbc, SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt));
    }

    virtual void TearDown() override {
        if (hstmt) {
            ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeHandle(SQL_HANDLE_STMT, hstmt));
            hstmt = nullptr;
        }

        if (hdbc) {
            ODBC_CALL_ON_DBC_LOG(hdbc, SQLDisconnect(hdbc));
            ODBC_CALL_ON_DBC_THROW(hdbc, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));
            hdbc = nullptr;
        }

        if (henv) {
            ODBC_CALL_ON_ENV_THROW(henv, SQLFreeHandle(SQL_HANDLE_ENV, henv));
            henv = nullptr;
        }
    }
    
    virtual ~StatementParameterBinding() {
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
    SQLHENV henv = nullptr;
    SQLHDBC hdbc = nullptr;
    SQLHSTMT hstmt = nullptr;
};

TEST_F(StatementParameterBinding, String) {
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, (SQLTCHAR*) "SELECT name FROM system.contributors WHERE name LIKE ? ORDER BY name LIMIT 10", SQL_NTS));
    
    SQLCHAR param[256] = {};
    SQLLEN param_ind = 0;
    
    char * param_ptr = reinterpret_cast<char *>(param);
    std::strncpy(param_ptr, "%q%", lengthof(param));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, std::strlen(param_ptr), 0, param_ptr, lengthof(param), &param_ind));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
    
    bool no_results = true;

    while (true) {
        SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
            SQLCHAR value[256] = {};
            SQLLEN value_ind = 0;

            ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(hstmt, 1, SQL_C_CHAR, &value, lengthof(value), &value_ind));

            ASSERT_TRUE(value_ind >= 0 || value_ind == SQL_NTS);

            const char * value_ptr = reinterpret_cast<const char *>(value);
            const auto value_str = (value_ind == SQL_NTS ? std::string(value_ptr) : std::string(value_ptr, value_ind));

            std::cout << value_str << std::endl;
            no_results = false;
        }
        else {
            break;
        }
    }

    ASSERT_FALSE(no_results);
}

TEST_F(StatementParameterBinding, GUID) {
/*
// TODO: create the table (using SQLExecDirect() ?), and modify this query.
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, (SQLTCHAR*) "SELECT name FROM system.contributors WHERE name LIKE ? ORDER BY name", SQL_NTS));
    
    SQLGUID param;
 
// TODO: populate 'param' with some GUID (binary; see the SQLGUID structure)
 
    SQLLEN param_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_GUID, SQL_GUID, 0, 0, &param, sizeof(param), &param_ind));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    bool no_results = true;

    while (true) {
        SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_ERROR || rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
            SQLCHAR value[256] = {};
            SQLLEN value_ind = 0;

            ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(hstmt, 1, SQL_C_CHAR, &value, lengthof(value), &value_ind));

            ASSERT_TRUE(value_ind >= 0 || value_ind == SQL_NTS);
 
            const char * value_ptr = reinterpret_cast<const char *>(value);
            const auto value_str = (value_ind == SQL_NTS ? std::string(value_ptr) : std::string(value_ptr, value_ind));

// TODO: assuming only there is a single string column in the results - we read it here. Check that the value is the expected one.
            std::cout << value_str << std::endl;
            no_results = false;
        }
        else {
            break;
        }
    }

    ASSERT_FALSE(no_results);
*/
}
