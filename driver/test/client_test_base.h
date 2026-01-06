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
        char query_id_data[37] = {};
        SQLINTEGER query_id_len{};
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(
            hstmt,
            SQL_CH_STMT_ATTR_LAST_QUERY_ID,
            query_id_data,
            SQL_LEN_BINARY_ATTR(std::ssize(query_id_data)),
            &query_id_len));
        return toUTF8(query_id_data);
    }

    std::optional<std::string> singleStringQuery(const std::string & query)
    {
        static constexpr size_t max_result_length = 1024;
        auto query_encoded = fromUTF8<PTChar>(query);

        HSTMT query_stmt = nullptr;
        ODBC_CALL_ON_DBC_THROW(hdbc, SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &query_stmt));
        auto free_stmt = [](SQLHSTMT stmt) { SQLFreeHandle(SQL_HANDLE_STMT, stmt); };
        auto query_stmt_cleanup = std::unique_ptr<void, decltype(free_stmt)>(query_stmt, free_stmt);

        std::basic_string<PTChar> out(max_result_length, '\0');
        SQLLEN out_len = 0;
        ODBC_CALL_ON_STMT_THROW(query_stmt, SQLExecDirect(query_stmt, ptcharCast(query_encoded.data()), SQL_NTS));

        auto fetch_res = SQLFetch(query_stmt);
        if (fetch_res == SQL_NO_DATA) {
            return std::nullopt;
        }
        ODBC_CALL_ON_STMT_THROW(query_stmt, fetch_res);

        ODBC_CALL_ON_STMT_THROW(query_stmt, SQLGetData(
            query_stmt,
            1,
            getCTypeFor<SQLTCHAR*>(),
            out.data(),
            out.size(),
            &out_len
        ));
        out.resize(out_len / sizeof(PTChar));
        return toUTF8(out);
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

#define STMT_OK(expr) ODBC_CALL_ON_STMT_THROW(hstmt, (expr));
