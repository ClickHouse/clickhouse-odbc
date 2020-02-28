#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>

#include <iostream>
#include <string>

#include <cstring>

class StatementParametersTest
    : public ClientTestBase
{
};

TEST_F(StatementParametersTest, BindingMissing) {
    const auto query = fromUTF8<SQLTCHAR>("SELECT isNull(?)");
    auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));
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

    ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
}

TEST_F(StatementParametersTest, BindingNoBuffer) {
    const auto query = fromUTF8<SQLTCHAR>("SELECT isNull(?)");
    auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

    SQLINTEGER param = 0;
    SQLLEN param_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));
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

    ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
}

TEST_F(StatementParametersTest, BindingNullStringValueForInteger) {
    const auto query = fromUTF8<SQLTCHAR>("SELECT isNull(?)");
    auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

#if defined(_IODBCUNIX_H)
    // iODBC workaround: disable potential use of SQLWCHAR in this test case,
    // since iODBC, for reasons unknown, changes the 4th argument of SQLBindParameter()
    // from SQL_C_WCHAR to SQL_C_CHAR, if this client is Unicode and the driver pointed by DSN is ANSI,
    // but does not convert the actual buffer (naturally). This makes the driver unable to interpret the buffer correctly.
    // TODO: eventually review and fix or report a defect on iODBC, if it doesn't have any reasonable explanation.
#    define SQLmyTCHAR SQLCHAR
#    define SQL_C_myTCHAR SQL_C_CHAR
#else
#    define SQLmyTCHAR SQLTCHAR
#    define SQL_C_myTCHAR SQL_C_TCHAR
#endif

    std::basic_string<SQLmyTCHAR> param;
    SQLLEN param_ind = 0;

    fromUTF8<SQLmyTCHAR>("Null", param);
    auto * param_wptr = const_cast<SQLmyTCHAR *>(param.c_str());

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindParameter(
            hstmt,
            1,
            SQL_PARAM_INPUT,
            SQL_C_myTCHAR,
            SQL_INTEGER,
            param.size(),
            0,
            param_wptr,
            param.size() * sizeof(SQLTCHAR),
            &param_ind
        )
    );

#undef SQLmyTCHAR
#undef SQL_C_myTCHAR

    // TODO: Workaround for workaround for https://github.com/ClickHouse/ClickHouse/issues/7488 . Remove when sorted-out.
    // Strictly speaking, this is not allowed, and parameters must always be nullable.
    SQLHDESC hdesc = 0;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_PARAM_DESC, &hdesc, 0, NULL));
    ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_NULLABLE, reinterpret_cast<SQLPOINTER>(SQL_NULLABLE), 0));

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

    ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
}

TEST_F(StatementParametersTest, BindingNullStringValueForString) {
    const auto query = fromUTF8<SQLTCHAR>("SELECT isNull(?)");
    auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

#if defined(_IODBCUNIX_H)
    // iODBC workaround: disable potential use of SQLWCHAR in this test case,
    // since iODBC, for reasons unknown, changes the 4th argument of SQLBindParameter()
    // from SQL_C_WCHAR to SQL_C_CHAR, if this client is Unicode and the driver pointed by DSN is ANSI,
    // but does not convert the actual buffer (naturally). This makes the driver unable to interpret the buffer correctly.
    // TODO: eventually review and fix or report a defect on iODBC, if it doesn't have any reasonable explanation.
#    define SQLmyTCHAR SQLCHAR
#    define SQL_C_myTCHAR SQL_C_CHAR
#else
#    define SQLmyTCHAR SQLTCHAR
#    define SQL_C_myTCHAR SQL_C_TCHAR
#endif

    std::basic_string<SQLmyTCHAR> param;
    SQLLEN param_ind = 0;

    fromUTF8<SQLmyTCHAR>("Null", param);
    auto * param_wptr = const_cast<SQLmyTCHAR *>(param.c_str());

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindParameter(
            hstmt,
            1,
            SQL_PARAM_INPUT,
            SQL_C_myTCHAR,
            SQL_CHAR,
            param.size(),
            0,
            param_wptr,
            param.size() * sizeof(SQLTCHAR),
            &param_ind
        )
    );

#undef SQLmyTCHAR
#undef SQL_C_myTCHAR

    // TODO: Workaround for workaround for https://github.com/ClickHouse/ClickHouse/issues/7488 . Remove when sorted-out.
    // Strictly speaking, this is not allowed, and parameters must always be nullable.
    SQLHDESC hdesc = 0;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_PARAM_DESC, &hdesc, 0, NULL));
    ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_NULLABLE, reinterpret_cast<SQLPOINTER>(SQL_NULLABLE), 0));

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

    ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
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
        const auto query = fromUTF8<SQLTCHAR>("SELECT ?");
        auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

        SQLCHAR param[256] = {};
        SQLLEN param_ind = 0;

        char * param_ptr = reinterpret_cast<char *>(param);
        ASSERT_LT(initial_str.size(), lengthof(param));
        std::strncpy(param_ptr, initial_str.c_str(), lengthof(param) - 1);

        // We need this to autodetect actual precision and scale of the value in initial_str.
        SQL_NUMERIC_STRUCT param_typed;
        value_manip::to_null(param_typed);
        value_manip::from_value<std::string>::template to_value<SQL_NUMERIC_STRUCT>::convert(initial_str, param_typed);

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));
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
                convertSQLTypeToCType(SQL_DECIMAL), // TODO: should be SQL_ARD_TYPE but iODBC doesn't support it
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

        ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
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
        const auto query = fromUTF8<SQLTCHAR>("SELECT ?");
        auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

        T param;
        value_manip::to_null(param);
        value_manip::from_value<std::string>::template to_value<T>::convert(initial_str, param);

        SQLLEN param_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));
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
        value_manip::to_default(col);

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

        std::string resulting_str;
        value_manip::to_null(resulting_str);
        value_manip::from_value<T>::template to_value<std::string>::convert(col, resulting_str);

        if (case_sensitive)
            ASSERT_STREQ(resulting_str.c_str(), expected_str.c_str());
        else
            ASSERT_STRCASEEQ(resulting_str.c_str(), expected_str.c_str());

        ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
    }

    template <typename T>
    inline void do_execute(const std::string & initial_str, const std::string & expected_str, const TypeInfo& type_info, bool case_sensitive,
        typename std::enable_if<
            std::is_same<T, SQL_NUMERIC_STRUCT>::value
        >::type * = nullptr // T is SQL_NUMERIC_STRUCT
    ) {
        const auto query = fromUTF8<SQLTCHAR>("SELECT ?");
        auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

        T param;
        value_manip::to_null(param);
        value_manip::from_value<std::string>::template to_value<T>::convert(initial_str, param);

        SQLLEN param_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));
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
        value_manip::to_default(col);

        SQLLEN col_ind = 0;

        SQLHDESC hdesc = 0;
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &hdesc, 0, NULL));
        ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_TYPE, reinterpret_cast<SQLPOINTER>(getCTypeFor<decltype(col)>()), 0));
        ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_PRECISION, reinterpret_cast<SQLPOINTER>(param.precision), 0));
        ODBC_CALL_ON_DESC_THROW(hdesc, SQLSetDescField(hdesc, 1, SQL_DESC_SCALE, reinterpret_cast<SQLPOINTER>(param.scale), 0));

        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLGetData(
                hstmt,
                1,
                getCTypeFor<decltype(col)>(), // TODO: should be SQL_ARD_TYPE but iODBC doesn't support it
                &col,
                sizeof(col),
                &col_ind
            )
        );

        ASSERT_TRUE(col_ind >= 0 || col_ind == SQL_NTS);

        std::string resulting_str;
        value_manip::to_null(resulting_str);
        value_manip::from_value<T>::template to_value<std::string>::convert(col, resulting_str);

        if (case_sensitive)
            ASSERT_STREQ(resulting_str.c_str(), expected_str.c_str());
        else
            ASSERT_STRCASEEQ(resulting_str.c_str(), expected_str.c_str());

        ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
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

TEST_P(DISABLED_ParameterColumnRoundTripGUIDSymmetric, Execute) {
    execute<DataType>(GetParam(), GetParam(), type_info_for("UUID"), false/* case_sensitive */);
}

INSTANTIATE_TEST_SUITE_P(TypeConversion, DISABLED_ParameterColumnRoundTripGUIDSymmetric,
    ::testing::Values(
        "00000000-0000-0000-0000-000000000000",
        "01020304-0506-0708-090A-0B0C0D0E0F00",
        "10203040-5060-7080-90A0-B0C0D0E0F000",
        "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"
    )
);


using ParameterColumnRoundTripNumericSymmetric  = ParameterColumnRoundTripSymmetric<SQL_NUMERIC_STRUCT>;

TEST_P(ParameterColumnRoundTripNumericSymmetric,  Execute) {
    execute<DataType>(GetParam(), GetParam(), type_info_for("Decimal"));
}

INSTANTIATE_TEST_SUITE_P(TypeConversion, ParameterColumnRoundTripNumericSymmetric,
    ::testing::Values(
        "0",
        "12345",
        "-12345",
        "12345.6789",
        "-12345.6789",
        "12345.000000000000",
        "12345.001002003000",
        "100000000000000000",
        "-100000000000000000",
        ".000000000000000001",
        "-.000000000000000001",
        "999999999999999999",
        "-999999999999999999",
        ".999999999999999999",
        "-.999999999999999999"
    )
);


using ParameterColumnRoundTripNumericAsymmetric = ParameterColumnRoundTripAsymmetric<SQL_NUMERIC_STRUCT>;

TEST_P(ParameterColumnRoundTripNumericAsymmetric, Execute) {
    execute<DataType>(std::get<0>(GetParam()), std::get<1>(GetParam()), type_info_for("Decimal"));
}

INSTANTIATE_TEST_SUITE_P(TypeConversion, ParameterColumnRoundTripNumericAsymmetric,
    ::testing::ValuesIn(std::initializer_list<std::tuple<std::string, std::string>>{
        { "0.", "0" },
        { "-0.", "0" },
        { "0.000", ".000" },
        { "-0.000", ".000" },
        { "0001.00001", "1.00001" },
        { "-0001.00001", "-1.00001" },
        { "000000.123", ".123" }
    })
);


using ParameterColumnRoundTripDecimalAsStringSymmetric  = ParameterColumnRoundTripSymmetric<void>;

TEST_P(ParameterColumnRoundTripDecimalAsStringSymmetric, Execute) {
    execute_with_decimal_as_string(GetParam(), GetParam());
}

INSTANTIATE_TEST_SUITE_P(TypeConversion, ParameterColumnRoundTripDecimalAsStringSymmetric,
    ::testing::Values(

        // TODO: do DECIMALs have to not start with dot?

        "0",
        "12345",
        "-12345",
        "12345.6789",
        "-12345.6789",
        "12345.000000000000",
        "12345.001002003000",
        "100000000000000000",
        "-100000000000000000",
        ".000000000000000001",
        "-.000000000000000001",
        "999999999999999999",
        "-999999999999999999",
        ".999999999999999999",
        "-.999999999999999999"
    )
);
