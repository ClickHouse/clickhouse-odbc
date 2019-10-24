#include "platform.h"
#include "gtest_env.h"
#include "client_utils.h"

#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <cstring>

class StatementParametersTest
    : public ::testing::Test
{
public:
    virtual ~StatementParametersTest() {
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

protected:
    SQLHENV henv = nullptr;
    SQLHDBC hdbc = nullptr;
    SQLHSTMT hstmt = nullptr;
};

TEST_F(StatementParametersTest, BindingMissing) {
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, (SQLTCHAR*) "SELECT isNull(?)", SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
    SQLRETURN rc = SQLFetch(hstmt);

    if (rc == SQL_ERROR)
        throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

    if (rc == SQL_SUCCESS_WITH_INFO)
        std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

    if (!SQL_SUCCEEDED(rc))
        throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

    SQLINTEGER col = 0;
    SQLLEN col_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLGetData(
            hstmt,
            1,
            getCTypeFor<decltype(col)>(),
            &col,
            sizeof(col),
            &col_ind
        )
    );

    ASSERT_TRUE(col_ind >= 0 || col_ind == SQL_NTS);
    ASSERT_EQ(col, 1);
}

TEST_F(StatementParametersTest, BindingNoBuffer) {
    SQLINTEGER param = 0;
    SQLLEN param_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, (SQLTCHAR*) "SELECT isNull(?)", SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindParameter(
            hstmt,
            1,
            SQL_PARAM_INPUT,
            getCTypeFor<decltype(param)>(),
            SQL_INTEGER,
            0,
            0,
            nullptr, // N.B.: not &param here!
            sizeof(param),
            &param_ind
        )
    );
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
    SQLRETURN rc = SQLFetch(hstmt);

    if (rc == SQL_ERROR)
        throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

    if (rc == SQL_SUCCESS_WITH_INFO)
        std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

    if (!SQL_SUCCEEDED(rc))
        throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

    SQLINTEGER col = 0;
    SQLLEN col_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLGetData(
            hstmt,
            1,
            getCTypeFor<decltype(col)>(),
            &col,
            sizeof(col),
            &col_ind
        )
    );

    ASSERT_TRUE(col_ind >= 0 || col_ind == SQL_NTS);
    ASSERT_EQ(col, 1);
}

TEST_F(StatementParametersTest, BindingNullStringValueForInteger) {
    SQLCHAR param[16] = {};
    SQLLEN param_ind = 0;

    char * param_ptr = reinterpret_cast<char *>(param);
    std::strncpy(param_ptr, "Null", lengthof(param));

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, (SQLTCHAR*) "SELECT isNull(?)", SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindParameter(
            hstmt,
            1,
            SQL_PARAM_INPUT,
            SQL_C_CHAR,
            SQL_INTEGER,
            std::strlen(param_ptr),
            0,
            param_ptr,
            lengthof(param),
            &param_ind
        )
    );
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
    SQLRETURN rc = SQLFetch(hstmt);

    if (rc == SQL_ERROR)
        throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

    if (rc == SQL_SUCCESS_WITH_INFO)
        std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

    if (!SQL_SUCCEEDED(rc))
        throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

    SQLINTEGER col = 0;
    SQLLEN col_ind = 0;

    // Strictly speaking, this is not allowed, and parameters must always be nullable.
    SQLHDESC hdesc = 0;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_PARAM_DESC, &hdesc, 0, NULL));
    ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_NULLABLE, reinterpret_cast<SQLPOINTER>(SQL_NULLABLE), 0));

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLGetData(
            hstmt,
            1,
            getCTypeFor<decltype(col)>(),
            &col,
            sizeof(col),
            &col_ind
        )
    );

    ASSERT_TRUE(col_ind >= 0 || col_ind == SQL_NTS);
    ASSERT_EQ(col, 1);
}

TEST_F(StatementParametersTest, BindingNullStringValueForString) {
    SQLCHAR param[16] = {};
    SQLLEN param_ind = 0;

    char * param_ptr = reinterpret_cast<char *>(param);
    std::strncpy(param_ptr, "Null", lengthof(param));

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, (SQLTCHAR*) "SELECT isNull(?)", SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindParameter(
            hstmt,
            1,
            SQL_PARAM_INPUT,
            SQL_C_CHAR,
            SQL_CHAR,
            std::strlen(param_ptr),
            0,
            param_ptr,
            lengthof(param),
            &param_ind
        )
    );
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
    SQLRETURN rc = SQLFetch(hstmt);

    if (rc == SQL_ERROR)
        throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

    if (rc == SQL_SUCCESS_WITH_INFO)
        std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

    if (!SQL_SUCCEEDED(rc))
        throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

    SQLINTEGER col = 0;
    SQLLEN col_ind = 0;

    // Strictly speaking, this is not allowed, and parameters must always be nullable.
    SQLHDESC hdesc = 0;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_PARAM_DESC, &hdesc, 0, NULL));
    ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_NULLABLE, reinterpret_cast<SQLPOINTER>(SQL_NULLABLE), 0));

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLGetData(
            hstmt,
            1,
            getCTypeFor<decltype(col)>(),
            &col,
            sizeof(col),
            &col_ind
        )
    );

    ASSERT_TRUE(col_ind >= 0 || col_ind == SQL_NTS);
    ASSERT_EQ(col, 1);
}

class ParameterColumnRoundTrip
    : public StatementParametersTest
{
protected:
    template <typename T>
    inline auto execute(const std::string & initial_str, const std::string & expected_str, const TypeInfo& type_info, bool case_sensitive = true) {
        return do_execute<T>(initial_str, expected_str, type_info, case_sensitive);
    }

    inline auto execute_with_decimal_as_string(const std::string & initial_str, const std::string & expected_str, bool case_sensitive = true) {
        SQLCHAR param[256] = {};
        SQLLEN param_ind = 0;

        char * param_ptr = reinterpret_cast<char *>(param);
        std::strncpy(param_ptr, initial_str.c_str(), lengthof(param));

        // We need this to autodetect actual precision and scale of the value in initial_str.
        auto param_typed = value_manip::to<SQL_NUMERIC_STRUCT>::template from<std::string>(initial_str);

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, (SQLTCHAR*) "SELECT ?", SQL_NTS));
        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLBindParameter(
                hstmt,
                1,
                SQL_PARAM_INPUT,
                convertSQLTypeToCType(SQL_DECIMAL),
                SQL_DECIMAL,
                param_typed.precision,
                param_typed.scale,
                param_ptr,
                lengthof(param),
                &param_ind
            )
        );
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
        SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        SQLCHAR col[256] = {};
        SQLLEN col_ind = 0;

        SQLHDESC hdesc = 0;
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &hdesc, 0, NULL));
        ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_TYPE, reinterpret_cast<SQLPOINTER>(convertSQLTypeToCType(SQL_DECIMAL)), 0));
        ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_PRECISION, reinterpret_cast<SQLPOINTER>(param_typed.precision), 0));
        ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_SCALE, reinterpret_cast<SQLPOINTER>(param_typed.scale), 0));

        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLGetData(
                hstmt,
                1,
                SQL_ARD_TYPE,
                &col,
                sizeof(col),
                &col_ind
            )
        );

        ASSERT_TRUE(col_ind >= 0 || col_ind == SQL_NTS);

        char * col_ptr = reinterpret_cast<char *>(col);
        const auto resulting_str = std::string{col_ptr, static_cast<std::string::size_type>(col_ind)};

        if (case_sensitive)
            ASSERT_STREQ(resulting_str.c_str(), expected_str.c_str());
        else
            ASSERT_STRCASEEQ(resulting_str.c_str(), expected_str.c_str());
    }

private:
    template <typename T>
    inline void do_execute(const std::string & initial_str, const std::string & expected_str, const TypeInfo& type_info, bool case_sensitive,
        typename std::enable_if<
            std::is_pointer<T>::value
        >::type * = nullptr // T is a string type
    ) {
        throw std::runtime_error("not implemented");
    }

    template <typename T>
    inline void do_execute(const std::string & initial_str, const std::string & expected_str, const TypeInfo& type_info, bool case_sensitive,
        typename std::enable_if<
            !std::is_pointer<T>::value &&
            !std::is_same<T, SQL_NUMERIC_STRUCT>::value
        >::type * = nullptr // T is a struct (except SQL_NUMERIC_STRUCT) or scalar type
    ) {
        auto param = value_manip::to<T>::template from<std::string>(initial_str);
        SQLLEN param_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, (SQLTCHAR*) "SELECT ?", SQL_NTS));
        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLBindParameter(
                hstmt,
                1,
                SQL_PARAM_INPUT,
                getCTypeFor<decltype(param)>(),
                type_info.sql_type,
                value_manip::getColumnSize(param, type_info),
                value_manip::getDecimalDigits(param, type_info),
                &param,
                sizeof(param),
                &param_ind
            )
        );
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
        SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        T col;
        value_manip::reset(col);
        SQLLEN col_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLGetData(
                hstmt,
                1,
                getCTypeFor<decltype(col)>(),
                &col,
                sizeof(col),
                &col_ind
            )
        );

        ASSERT_TRUE(col_ind >= 0 || col_ind == SQL_NTS);

        const auto resulting_str = value_manip::to<std::string>::template from<T>(col);

        if (case_sensitive)
            ASSERT_STREQ(resulting_str.c_str(), expected_str.c_str());
        else
            ASSERT_STRCASEEQ(resulting_str.c_str(), expected_str.c_str());
    }

    template <typename T>
    inline void do_execute(const std::string & initial_str, const std::string & expected_str, const TypeInfo& type_info, bool case_sensitive,
        typename std::enable_if<
            std::is_same<T, SQL_NUMERIC_STRUCT>::value
        >::type * = nullptr // T is SQL_NUMERIC_STRUCT
    ) {
        auto param = value_manip::to<T>::template from<std::string>(initial_str);
        SQLLEN param_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, (SQLTCHAR*) "SELECT ?", SQL_NTS));
        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLBindParameter(
                hstmt,
                1,
                SQL_PARAM_INPUT,
                getCTypeFor<decltype(param)>(),
                type_info.sql_type,
                value_manip::getColumnSize(param, type_info),
                value_manip::getDecimalDigits(param, type_info),
                &param,
                sizeof(param),
                &param_ind
            )
        );
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
        SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        T col;
        value_manip::reset(col);
        SQLLEN col_ind = 0;

        SQLHDESC hdesc = 0;
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &hdesc, 0, NULL));
        ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_TYPE, reinterpret_cast<SQLPOINTER>(SQL_C_NUMERIC), 0));
        ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_PRECISION, reinterpret_cast<SQLPOINTER>(param.precision), 0));
        ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_SCALE, reinterpret_cast<SQLPOINTER>(param.scale), 0));

        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLGetData(
                hstmt,
                1,
                SQL_ARD_TYPE,
                &col,
                sizeof(col),
                &col_ind
            )
        );

        ASSERT_TRUE(col_ind >= 0 || col_ind == SQL_NTS);

        const auto resulting_str = value_manip::to<std::string>::template from<T>(col);

        if (case_sensitive)
            ASSERT_STREQ(resulting_str.c_str(), expected_str.c_str());
        else
            ASSERT_STRCASEEQ(resulting_str.c_str(), expected_str.c_str());
    }

};

template <typename T>
class ParameterColumnRoundTripSymmetric
    : public ParameterColumnRoundTrip
    , public ::testing::WithParamInterface<std::string>
{
protected:
    using DataType = T;
};

template <typename T>
class ParameterColumnRoundTripAsymmetric
    : public ParameterColumnRoundTrip
    , public ::testing::WithParamInterface<std::tuple<std::string, std::string>>
{
protected:
    using DataType = T;
};


// TODO: GIUD/UUID tests are temporarily disabled until this worked around/fixed: https://github.com/ClickHouse/ClickHouse/issues/7463
using DISABLED_ParameterColumnRoundTripGUIDSymmetric = ParameterColumnRoundTripSymmetric<SQLGUID>;
TEST_P(DISABLED_ParameterColumnRoundTripGUIDSymmetric, Execute) { execute<DataType>(GetParam(), GetParam(), type_info_for("UUID"), false/* case_sensitive */); }

INSTANTIATE_TEST_CASE_P(TypeConversion, DISABLED_ParameterColumnRoundTripGUIDSymmetric,
    ::testing::Values(
        "00000000-0000-0000-0000-000000000000",
        "01020304-0506-0708-090A-0B0C0D0E0F00",
        "10203040-5060-7080-90A0-B0C0D0E0F000",
        "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"
    )
);


using ParameterColumnRoundTripNumericSymmetric  = ParameterColumnRoundTripSymmetric<SQL_NUMERIC_STRUCT>;
TEST_P(ParameterColumnRoundTripNumericSymmetric,  Execute) { execute<DataType>(GetParam(), GetParam(), type_info_for("Decimal")); }

INSTANTIATE_TEST_CASE_P(TypeConversion, ParameterColumnRoundTripNumericSymmetric,
    ::testing::Values(
        "0",
        "12345",
        "-12345",
        "12345.6789",
        "-12345.6789",
        "12345.000000000000",
        "12345.001002003000",
        "10000000000000000000",
        "-10000000000000000000",
        ".00000000000000000001",
        "-.0000000000000000001",
        "9876543210987654321",
        ".9876543210987654321",
        "-9876543210987654321",
        "-.9876543210987654321",
        "9999999999999999999",
        "-9999999999999999999",
        ".9999999999999999999",
        "-.9999999999999999999",
        "18446744073709551615",
        "-18446744073709551615",
        ".18446744073709551615",
        "-.18446744073709551615"
    )
);


using ParameterColumnRoundTripNumericAsymmetric = ParameterColumnRoundTripAsymmetric<SQL_NUMERIC_STRUCT>;
TEST_P(ParameterColumnRoundTripNumericAsymmetric, Execute) { execute<DataType>(std::get<0>(GetParam()), std::get<1>(GetParam()), type_info_for("Decimal")); }

INSTANTIATE_TEST_CASE_P(TypeConversion, ParameterColumnRoundTripNumericAsymmetric,
    ::testing::Values(
        std::make_tuple("0.", "0"),
        std::make_tuple("-0.", "0"),
        std::make_tuple("0.000", ".000"),
        std::make_tuple("-0.000", ".000"),
        std::make_tuple("0001.00001", "1.00001"),
        std::make_tuple("-0001.00001", "-1.00001"),
        std::make_tuple("000000.123", ".123")
    )
);


using ParameterColumnRoundTripDecimalAsStringSymmetric  = ParameterColumnRoundTripSymmetric<SQLCHAR>;
TEST_P(ParameterColumnRoundTripDecimalAsStringSymmetric, Execute) { execute_with_decimal_as_string(GetParam(), GetParam()); }

INSTANTIATE_TEST_CASE_P(TypeConversion, ParameterColumnRoundTripDecimalAsStringSymmetric,
    ::testing::Values(
        "0",
        "12345",
        "-12345",
        "12345.6789",
        "-12345.6789",
        "12345.000000000000",
        "12345.001002003000",
        "10000000000000000000",
        "-10000000000000000000",
        "0.00000000000000000001",
        "-0.0000000000000000001",
        "9876543210987654321",
        "0.9876543210987654321",
        "-9876543210987654321",
        "-0.9876543210987654321",
        "9999999999999999999",
        "-9999999999999999999",
        "0.9999999999999999999",
        "-0.9999999999999999999",
        "18446744073709551615",
        "-18446744073709551615",
        "0.18446744073709551615",
        "-0.18446744073709551615"
    )
);
