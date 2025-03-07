#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <format>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <Poco/Foundation.h>

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
        std::make_tuple("AllGood_VerifyConnectionEarly_Off", "VerifyConnectionEarly=off",  FailOn::Never)
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

// Checks that each ClickHouse type is correctly mapped to a corresponding ODBC type
TEST_F(MiscellaneousTest, ClickhouseToSQLTypeMapping)
{

    struct TypeMappingTestEntry
    {
        std::string type;
        std::string input;
        SQLSMALLINT sql_type;
    };

    // FIXME(slabko): The commented out types are not supported by RowBinaryWithNamesAndTypes
    std::vector<TypeMappingTestEntry> types = {
        {"Bool", "0", SQL_VARCHAR},
        {"Int8", "0", SQL_TINYINT},
        {"UInt8", "0", SQL_TINYINT},
        {"Int16", "0", SQL_SMALLINT},
        {"UInt16", "0", SQL_SMALLINT},
        {"Int32", "0", SQL_INTEGER},
        {"UInt32", "0", SQL_BIGINT},
        {"Int64", "0", SQL_BIGINT},
        {"UInt64", "0", SQL_BIGINT},
        {"Int128", "0", SQL_VARCHAR},
        {"UInt128", "0", SQL_VARCHAR},
        {"Int256", "0", SQL_VARCHAR},
        {"UInt256", "0", SQL_VARCHAR},
        {"Float32", "0", SQL_REAL},
        {"Float64", "0", SQL_DOUBLE},
        {"Decimal(5)", "0", SQL_DECIMAL},
        {"Decimal32(5)", "0", SQL_DECIMAL},
        {"Decimal64(12)", "0", SQL_DECIMAL},
        {"Decimal128(24)", "0", SQL_DECIMAL},
        {"Decimal256(72)", "0", SQL_DECIMAL},
        {"String", "0", SQL_VARCHAR},
        {"FixedString(1)", "'0'", SQL_VARCHAR},
        {"Date", "0", SQL_TYPE_DATE},
        {"Date32", "0", SQL_VARCHAR},
        {"DateTime", "0", SQL_TYPE_TIMESTAMP},
        {"DateTime64", "0", SQL_TYPE_TIMESTAMP},
        {"UUID", "'00000000-0000-0000-0000-000000000000'", SQL_GUID},
        {"IPv4", "'0.0.0.0'", SQL_VARCHAR},
        {"IPv6", "'::'", SQL_VARCHAR},
        {"Array(Int32)", "[1,2,3]", SQL_VARCHAR},
        {"Tuple(Int32, Int32)", "(1,2)", SQL_VARCHAR},
        {"LowCardinality(String)", "'0'", SQL_VARCHAR},
        {"LowCardinality(Int32)", "0", SQL_INTEGER},
        {"LowCardinality(DateTime)", "0", SQL_TYPE_TIMESTAMP},
        {"Enum('hello' = 0, 'world' = 1)", "'hello'", SQL_VARCHAR},
    };

    std::unordered_map<std::string, SQLSMALLINT> sql_types{};
    std::stringstream query_stream;
    query_stream << "SELECT";
    for(const auto& [type, input, sql_type] : types) {
        auto type_escaped = Poco::replace(type, "'", "\\'");
        sql_types[std::string(type)] = sql_type;
        query_stream << std::format(" CAST({}, '{}') as `{}`,", input, type_escaped, type);
    }
    query_stream.seekp(-1, std::stringstream::cur) << " ";
    query_stream << "SETTINGS allow_suspicious_low_cardinality_types = 1";

    auto query = fromUTF8<SQLTCHAR>(query_stream.str());

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query.data(), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

	SQLSMALLINT num_columns{};
	ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, &num_columns));

    SQLSMALLINT name_length = 0;
    SQLSMALLINT data_type = 0;

    std::basic_string<SQLTCHAR> input_name(256, '\0');
    for (SQLSMALLINT column = 1; column <= num_columns; ++column) {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLDescribeCol(
            hstmt,
            column,
            input_name.data(),
            static_cast<SQLSMALLINT>(input_name.size()),
            &name_length,
            &data_type,
            nullptr,
            nullptr,
            nullptr));
        std::string name(input_name.begin(), input_name.begin() + name_length);
        ASSERT_EQ(sql_types[name], data_type) << "type: " << name;
    }
}
