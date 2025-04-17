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
        size = 2;
        rc = ODBC_CALL_ON_STMT_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)size, 0));
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        size = 123;
        rc = ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, &size, sizeof(size), 0));
        ASSERT_EQ(size, 2);
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

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, query_wptr, SQL_NTS));

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

TEST_F(MiscellaneousTest, NullableNothing) {
    const std::string query_orig = "SELECT NULL AS col";

    const auto query = fromUTF8<SQLTCHAR>(query_orig);
    auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, query_wptr, SQL_NTS));

    SQLSMALLINT sql_type = SQL_BIT;
    SQLSMALLINT nullable = SQL_NULLABLE_UNKNOWN;

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLDescribeCol(hstmt, 1, NULL, 0, NULL, &sql_type, NULL, NULL, &nullable));

    EXPECT_EQ(sql_type, SQL_TYPE_NULL);
    EXPECT_EQ(nullable, SQL_NULLABLE);
}

enum class FailOn {
    Connect,
    Execute,
    Never
};

class ConnectionFailureReporing
    : public ClientTestWithParamBase<
        std::tuple<
            std::string, // parameter set name
            std::string, // extra name=value semicolon-separated string to append to the connection string
            FailOn       // when to expect the failure, if any
        >
    >
{
private:
    using Base = ClientTestWithParamBase<std::tuple<std::string, std::string, FailOn>>;

protected:
    virtual void SetUp() override {
        Base::SetUp();

        // As a precondition, check that by default the server is reachable,
        // and we are able connect, authenticate, and execute queries successfully.

        const std::string query_orig = "SELECT 1";

        const auto query = fromUTF8<SQLTCHAR>(query_orig);
        auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, query_wptr, SQL_NTS));

        // Free the original Connection and Statement instances, and create a new Connection,
        // but don't connect it yet - each test will do it on its own.

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeHandle(SQL_HANDLE_STMT, hstmt));
        hstmt = nullptr;

        SQLDisconnect(hdbc);
        ODBC_CALL_ON_DBC_THROW(hdbc, SQLFreeHandle(SQL_HANDLE_DBC, hdbc));

        ODBC_CALL_ON_ENV_THROW(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc));
    }
};

TEST_P(ConnectionFailureReporing, TryQuery) {
    const auto & [/* unused */name, cs_extras, fail_on] = GetParam();

    {
        const auto & dsn_orig = TestEnvironment::getInstance().getDSN();
        std::string cs_orig = "DSN=" + dsn_orig + ";" + cs_extras;
        const auto cs = fromUTF8<SQLTCHAR>(cs_orig);
        auto * cs_wptr = const_cast<SQLTCHAR *>(cs.c_str());

        const auto rc = SQLDriverConnect(hdbc, NULL, cs_wptr, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);

        if (fail_on == FailOn::Connect) {
            ASSERT_EQ(rc, SQL_ERROR) << "Expected to fail on Connect!";
            return;
        }
        else {
            ODBC_CALL_ON_DBC_THROW(hdbc, rc);
        }
    }

    ODBC_CALL_ON_DBC_THROW(hdbc, SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt));

    {
        const std::string query_orig = "SELECT 1";

        const auto query = fromUTF8<SQLTCHAR>(query_orig);
        auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));

        const auto rc = SQLExecute(hstmt);

        if (fail_on == FailOn::Execute) {
            ASSERT_EQ(rc, SQL_ERROR) << "Expected to fail on Execute!";
            return;
        }
        else {
            ODBC_CALL_ON_STMT_THROW(hstmt, rc);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    MiscellaneousTest,
    ConnectionFailureReporing,
    ::testing::Values(
        std::make_tuple("BadHost_FailOnConnect",     "Host=some_bad_hostname;VerifyConnectionEarly=on", FailOn::Connect),
        std::make_tuple("BadUsername_FailOnConnect", "UID=some_bad_username;VerifyConnectionEarly=on",  FailOn::Connect),
        std::make_tuple("BadPassword_FailOnConnect", "PWD=some_bad_password;VerifyConnectionEarly=on",  FailOn::Connect),

        std::make_tuple("BadHost_FailOnExecute",     "Host=some_bad_hostname;VerifyConnectionEarly=off", FailOn::Execute),
        std::make_tuple("BadUsername_FailOnExecute", "UID=some_bad_username;VerifyConnectionEarly=off",  FailOn::Execute),
        std::make_tuple("BadPassword_FailOnExecute", "PWD=some_bad_password;VerifyConnectionEarly=off",  FailOn::Execute),

        std::make_tuple("BadHost_FailOnExecuteByDefault",     "Host=some_bad_hostname", FailOn::Execute),
        std::make_tuple("BadUsername_FailOnExecuteByDefault", "UID=some_bad_username",  FailOn::Execute),
        std::make_tuple("BadPassword_FailOnExecuteByDefault", "PWD=some_bad_password",  FailOn::Execute),

        std::make_tuple("BadHost_FailOnExecuteByDefault2",     "Host=some_bad_hostname;VerifyConnectionEarly=", FailOn::Execute),
        std::make_tuple("BadUsername_FailOnExecuteByDefault2", "UID=some_bad_username;VerifyConnectionEarly=",  FailOn::Execute),
        std::make_tuple("BadPassword_FailOnExecuteByDefault2", "PWD=some_bad_password;VerifyConnectionEarly=",  FailOn::Execute),

        std::make_tuple("AllGood_VerifyConnectionEarly_Empty", "VerifyConnectionEarly=",   FailOn::Never),
        std::make_tuple("AllGood_VerifyConnectionEarly_On",  "VerifyConnectionEarly=on",   FailOn::Never),
        std::make_tuple("AllGood_VerifyConnectionEarly_Off", "VerifyConnectionEarly=off",  FailOn::Never),

        std::make_tuple("AllGood_AutoSessionId_Empty", "AutoSessionId=",   FailOn::Never),
        std::make_tuple("AllGood_AutoSessionId_On",  "AutoSessionId=on",   FailOn::Never),
        std::make_tuple("AllGood_AutoSessionId_Off", "AutoSessionId=off",  FailOn::Never)
    ),
    [] (const auto & param_info) {
        return std::get<0>(param_info.param);
    }
);

class HugeIntTypeReporting
    : public ClientTestWithParamBase<
        std::tuple<
            std::string,     // original "huge" integer type
            std::tuple<
                std::string, // parameter set name
                std::string, // extra name=value semicolon-separated string to append to the connection string
                SQLSMALLINT  // expected reported column type
            >
        >
    >
{
private:
    using Base = ClientTestWithParamBase<std::tuple<std::string, std::tuple<std::string, std::string, SQLSMALLINT>>>;

public:
    HugeIntTypeReporting()
        : Base(/*skip_connect = */true)
    {
    }

    void connect(const std::string & connection_string) {
        ASSERT_EQ(hstmt, nullptr);

        const auto cs = fromUTF8<SQLTCHAR>(connection_string);
        auto * cs_wptr = const_cast<SQLTCHAR *>(cs.c_str());

        ODBC_CALL_ON_DBC_THROW(hdbc, SQLDriverConnect(hdbc, NULL, cs_wptr, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT));
        ODBC_CALL_ON_DBC_THROW(hdbc, SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt));
    }
};

TEST_P(HugeIntTypeReporting, Check) {
    const auto & [type, params] = GetParam();
    const auto & [/* unused */name, cs_extras, expected_sql_type] = params;

    const auto & dsn = TestEnvironment::getInstance().getDSN();
    const auto cs = "DSN=" + dsn + ";" + cs_extras;
    connect(cs);

    const auto query_orig = "SELECT CAST('0', '" + type + "') AS col";

    const auto query = fromUTF8<SQLTCHAR>(query_orig);
    auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, query_wptr, SQL_NTS));

    SQLLEN sql_type = SQL_TYPE_NULL;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLColAttribute(hstmt, 1, SQL_DESC_TYPE, NULL, 0, NULL, &sql_type));
    ASSERT_EQ(sql_type, expected_sql_type);
}

INSTANTIATE_TEST_SUITE_P(
    MiscellaneousTest,
    HugeIntTypeReporting,
    ::testing::Combine(
        ::testing::Values(

            // TODO: uncomment each type once its support is implemented.
            "UInt64"/*, "Int128", "UInt128", "Int256", "UInt256"*/

        ),
        ::testing::Values(
            std::make_tuple("HugeIntAsString_Default", "",                     SQL_BIGINT),
            std::make_tuple("HugeIntAsString_Empty",   "HugeIntAsString=",     SQL_BIGINT),
            std::make_tuple("HugeIntAsString_On",      "HugeIntAsString=on",   SQL_VARCHAR),
            std::make_tuple("HugeIntAsString_Off",     "HugeIntAsString=off",  SQL_BIGINT)
        )
    ),
    [] (const auto & param_info) {
        return std::get<0>(std::get<1>(param_info.param)) + "_with_" + std::get<0>(param_info.param);
    }
);
