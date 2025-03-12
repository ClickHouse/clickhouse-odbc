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
        ///  12345 is valid for "12345.000000000000",
        ///  and 12345.001002003 is valid for "12345.001002003000",
        if (resulting_str.size() < expected_str.size())
        {
            if (expected_str.substr(0, resulting_str.size()) == resulting_str)
            {
                if (std::all_of(expected_str.begin() + resulting_str.size(), expected_str.end(), [](auto c) { return c == '0' || c == '.'; }))
                {
                    ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
                    return;
                }
            }

        }

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
    execute<DataType>(GetParam(), GetParam(), typeInfoFor("UUID"), false/* case_sensitive */);
}

INSTANTIATE_TEST_SUITE_P(TypeConversion, DISABLED_ParameterColumnRoundTripGUIDSymmetric,
    ::testing::Values(
        "00000000-0000-0000-0000-000000000000",
        "01020304-0506-0708-090A-0B0C0D0E0F00",
        "10203040-5060-7080-90A0-B0C0D0E0F000",
        "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"
    )
);


using ParameterColumnRoundTripNumericSymmetric = ParameterColumnRoundTripSymmetric<SQL_NUMERIC_STRUCT>;

TEST_P(ParameterColumnRoundTripNumericSymmetric, Execute) {
    execute<DataType>(GetParam(), GetParam(), typeInfoFor("Decimal"));
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
    execute<DataType>(std::get<0>(GetParam()), std::get<1>(GetParam()), typeInfoFor("Decimal"));
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


using ParameterColumnRoundTripDecimalAsStringSymmetric = ParameterColumnRoundTripSymmetric<void>;

TEST_P(ParameterColumnRoundTripDecimalAsStringSymmetric, Execute) {
    execute_with_decimal_as_string(GetParam(), GetParam());
}

INSTANTIATE_TEST_SUITE_P(TypeConversion, ParameterColumnRoundTripDecimalAsStringSymmetric,
    ::testing::Values(

    // TODO: add cases with 0 whole part. Currently the unified testing doesn't play well with the
    // different wire formats with enabled conservative value conversions.

        "0",
        "12345",
        "-12345",
        "12345.6789",
        "-12345.6789",
        "12345",
        "12345.001002003",
        "100000000000000000",
        "-100000000000000000",
        "1.00000000000000001",
        "-1.00000000000000001",
        "999999999999999999",
        "-999999999999999999",
        "1.99999999999999999",
        "-1.99999999999999999"
    )
);
