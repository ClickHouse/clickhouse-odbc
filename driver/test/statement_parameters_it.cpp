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

struct ParameterArrayStrideCase {
    SQLSMALLINT sql_type;
    const char * column_type;
    bool row_wise;
};

class ParameterArrayStrideTest : public ClientTestWithParamBase<ParameterArrayStrideCase> {};

TEST_P(ParameterArrayStrideTest, InsertsEachBoundValue) {
    const auto & [sql_type, column_type, row_wise] = GetParam();
    auto create = fromUTF8<PTChar>(
        std::string("CREATE OR REPLACE TABLE array_stride (id Int32, v ") +
        column_type + ") ENGINE = MergeTree ORDER BY id");
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(create.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));

    SQLINTEGER ids[] = {1, 2, 3, 4};
    // Four input elements followed by initialized padding.
    SQLINTEGER values[] = {101, 202, 303, 404, 505, 606, 707, 808};
    SQLLEN indicators[lengthof(ids)] = {};
    struct Row {
        SQLINTEGER id;
        SQLINTEGER value;
        SQLLEN id_indicator;
        SQLLEN value_indicator;
    } rows[] = {{1, 101, 0, 0}, {2, 202, 0, 0}, {3, 303, 0, 0}, {4, 404, 0, 0}};

    auto insert = fromUTF8<PTChar>("INSERT INTO array_stride (id, v) VALUES (?, ?)");
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(insert.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER)lengthof(ids), 0));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLSetStmtAttr(
        hstmt, SQL_ATTR_PARAM_BIND_TYPE, (SQLPOINTER)(row_wise ? sizeof(Row) : SQL_PARAM_BIND_BY_COLUMN), 0));

    const auto bind = [&](SQLUSMALLINT number, SQLSMALLINT type, SQLINTEGER * value, SQLLEN * indicator) {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
            hstmt, number, SQL_PARAM_INPUT, SQL_C_SLONG, type, 20, 0, value, 0, indicator));
    };
    bind(1, SQL_INTEGER, row_wise ? &rows[0].id : ids, row_wise ? &rows[0].id_indicator : indicators);
    bind(2, sql_type, row_wise ? &rows[0].value : values, row_wise ? &rows[0].value_indicator : indicators);
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));

    auto select = fromUTF8<PTChar>("SELECT id, toInt64(v) FROM array_stride ORDER BY id");
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(select.data()), SQL_NTS));
    for (std::size_t i = 0; i < lengthof(ids); ++i) {
        SQLINTEGER id = 0;
        SQLBIGINT value = 0;
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(hstmt, 1, SQL_C_SLONG, &id, sizeof(id), nullptr));
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(hstmt, 2, SQL_C_SBIGINT, &value, sizeof(value), nullptr));
        EXPECT_EQ(id, ids[i]);
        EXPECT_EQ(value, values[i]) << "parameter set " << i;
    }
    EXPECT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
}

INSTANTIATE_TEST_SUITE_P(Arrays, ParameterArrayStrideTest, ::testing::Values(
    ParameterArrayStrideCase{SQL_BIGINT, "Int64", false},
    ParameterArrayStrideCase{SQL_VARCHAR, "String", false},
    ParameterArrayStrideCase{SQL_INTEGER, "Int32", false},
    ParameterArrayStrideCase{SQL_BIGINT, "Int64", true}
));

class ParameterColumnRoundTrip
    : public StatementParametersTest
{
protected:
    template <typename T>
    inline auto execute(const std::string & initial_str, const std::string & expected_str, const TypeInfo& type_info, bool case_sensitive = true) {
        return do_execute<T>(initial_str, expected_str, type_info, case_sensitive);
    }

    inline auto execute_with_decimal_as_string(const std::string & initial_str, const std::string & expected_str, bool case_sensitive = true) {
        auto query = fromUTF8<PTChar>("SELECT ?");

        SQLCHAR param[256] = {};
        SQLLEN param_ind = 0;

        char * param_ptr = reinterpret_cast<char *>(param);
        ASSERT_LT(initial_str.size(), lengthof(param));
        std::strncpy(param_ptr, initial_str.data(), lengthof(param) - 1);

        // We need this to autodetect actual precision and scale of the value in initial_str.
        SQL_NUMERIC_STRUCT param_typed;
        value_manip::to_null(param_typed);
        value_manip::from_value<std::string>::template to_value<SQL_NUMERIC_STRUCT>::convert(initial_str, param_typed);

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
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
            ASSERT_STREQ(resulting_str.data(), expected_str.data());
        else
            ASSERT_STRCASEEQ(resulting_str.data(), expected_str.data());

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
        auto query = fromUTF8<PTChar>("SELECT ?");

        T param;
        value_manip::to_null(param);
        value_manip::from_value<std::string>::template to_value<T>::convert(initial_str, param);

        SQLLEN param_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLBindParameter(
                hstmt,
                1,
                SQL_PARAM_INPUT,
                getCTypeFor<decltype(param)>(),
                type_info.data_type,
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
            ASSERT_STREQ(resulting_str.data(), expected_str.data());
        else
            ASSERT_STRCASEEQ(resulting_str.data(), expected_str.data());

        ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
    }

    template <typename T>
    inline void do_execute(const std::string & initial_str, const std::string & expected_str, const TypeInfo& type_info, bool case_sensitive,
        typename std::enable_if<
            std::is_same<T, SQL_NUMERIC_STRUCT>::value
        >::type * = nullptr // T is SQL_NUMERIC_STRUCT
    ) {
        auto query = fromUTF8<PTChar>("SELECT ?");

        T param;
        value_manip::to_null(param);
        value_manip::from_value<std::string>::template to_value<T>::convert(initial_str, param);

        SQLLEN param_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLBindParameter(
                hstmt,
                1,
                SQL_PARAM_INPUT,
                getCTypeFor<decltype(param)>(),
                type_info.data_type,
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
            ASSERT_STREQ(resulting_str.data(), expected_str.data());
        else
            ASSERT_STRCASEEQ(resulting_str.data(), expected_str.data());

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


using ParameterColumnRoundTripFloat64Symmetric = ParameterColumnRoundTripSymmetric<SQLDOUBLE>;

TEST_P(ParameterColumnRoundTripFloat64Symmetric, Execute) {
    execute<DataType>(GetParam(), GetParam(), typeInfoFor("Float64"));
}

// The server parses the parameter text. Server 25.6 (test/docker-compose.yml) parses some texts with
// an error of 1 ULP, for example "9.99e-07", and "3.4028235e+38" as Float32. Server 26.10 parses them
// exactly. Use only values that the CI server parses exactly, in this list and in the Float32 list.
// type_conversion_ut checks the driver text of "9.99e-07" and "3.4028235e+38" without a server.
INSTANTIATE_TEST_SUITE_P(TypeConversion, ParameterColumnRoundTripFloat64Symmetric,
    ::testing::Values(
        "0",
        "-0",
        "42",
        "-2.5",
        "0.1",
        "123.456789123",
        "0.3333333333333333",
        "0.000001",
        "9.5e-07",
        "1e-07",
        "-0.000001234",
        "2.5e-10",
        "999000000000000000000",
        "1e+21",
        "1.7976931348623157e+308"
    )
);


using ParameterColumnRoundTripFloat32Symmetric = ParameterColumnRoundTripSymmetric<SQLREAL>;

TEST_P(ParameterColumnRoundTripFloat32Symmetric, Execute) {
    execute<DataType>(GetParam(), GetParam(), typeInfoFor("Float32"));
}

// See the comment on the Float64 list.
INSTANTIATE_TEST_SUITE_P(TypeConversion, ParameterColumnRoundTripFloat32Symmetric,
    ::testing::Values(
        "0",
        "-2.5",
        "0.1",
        "123.45679",
        "1e-07",
        "3e+38"
    )
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
