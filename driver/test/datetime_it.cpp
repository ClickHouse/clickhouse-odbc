#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <iostream>

class DateTime
    : public ClientTestWithParamBase<
        std::tuple<
            std::string,         // parameter set name

            // TODO: remove this once the formats behave identically.
            std::string,         // format to use
            std::string,         // local timezone to use

            std::string,         // expression for SELECT
            SQLSMALLINT,         // expected reported column type
            std::string,         // value, when retrieved as string
            SQL_TIMESTAMP_STRUCT // value, when retrieved as SQL_TIMESTAMP_STRUCT
        >
    >
{
};

bool operator== (const SQL_DATE_STRUCT & lhs, const SQL_DATE_STRUCT & rhs) {
    return (
        lhs.year == rhs.year &&
        lhs.month == rhs.month &&
        lhs.day == rhs.day
    );
}

bool operator== (const SQL_TIME_STRUCT & lhs, const SQL_TIME_STRUCT & rhs) {
    return (
        lhs.hour == rhs.hour &&
        lhs.minute == rhs.minute &&
        lhs.second == rhs.second
    );
}

bool operator== (const SQL_TIMESTAMP_STRUCT & lhs, const SQL_TIMESTAMP_STRUCT & rhs) {
    return (
        lhs.year == rhs.year &&
        lhs.month == rhs.month &&
        lhs.day == rhs.day &&
        lhs.hour == rhs.hour &&
        lhs.minute == rhs.minute &&
        lhs.second == rhs.second &&
        lhs.fraction == rhs.fraction
    );
}

TEST_P(DateTime, GetData) {
    const auto & [
        name, // unused here
        format,
        local_tz,
        expr,
        expected_sql_type,
        expected_str_val,
        expected_timestamp_val
    ] = GetParam();

    const SQL_DATE_STRUCT expected_date_val = {
        expected_timestamp_val.year,
        expected_timestamp_val.month,
        expected_timestamp_val.day
    };

    const SQL_TIME_STRUCT expected_time_val = {
        expected_timestamp_val.hour,
        expected_timestamp_val.minute,
        expected_timestamp_val.second
    };

    const auto orig_local_tz = get_env_var("TZ");
    set_env_var("TZ", local_tz);
    try {

    const std::string query_orig = "SELECT " + expr + " AS col FORMAT " + format;

    const auto query = fromUTF8<SQLTCHAR>(query_orig);
    auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, query_wptr, SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));

    {
        SQLLEN sql_type = SQL_TYPE_NULL;
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLColAttribute(hstmt, 1, SQL_DESC_TYPE, NULL, 0, NULL, &sql_type));
        EXPECT_EQ(sql_type, expected_sql_type);
    }

    {
        SQLTCHAR col[64] = {};
        SQLLEN col_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(
            hstmt,
            1,
            getCTypeFor<decltype(&col[0])>(),
            &col,
            sizeof(col),
            &col_ind
        ));

        EXPECT_EQ(toUTF8(col), expected_str_val);
    }

    if (format != "RowBinaryWithNamesAndTypes" || expected_sql_type == SQL_TYPE_DATE) {
        SQL_DATE_STRUCT col = {};
        SQLLEN col_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(
            hstmt,
            1,
            getCTypeFor<decltype(col)>(),
            &col,
            sizeof(col),
            &col_ind
        ));

        EXPECT_EQ(col, expected_date_val);
    }

    if (format != "RowBinaryWithNamesAndTypes") {
        SQL_TIME_STRUCT col = {};
        SQLLEN col_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(
            hstmt,
            1,
            getCTypeFor<decltype(col)>(),
            &col,
            sizeof(col),
            &col_ind
        ));

        EXPECT_EQ(col, expected_time_val);
    }

    if (format != "RowBinaryWithNamesAndTypes" || expected_sql_type != SQL_TYPE_DATE) {
        SQL_TIMESTAMP_STRUCT col = {};
        SQLLEN col_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(
            hstmt,
            1,
            getCTypeFor<decltype(col)>(),
            &col,
            sizeof(col),
            &col_ind
        ));

        EXPECT_EQ(col, expected_timestamp_val);
    }

    }
    catch (...) {
        try {
            set_env_var("TZ", orig_local_tz);
        }
        catch (const std::exception & ex) {
            std::cerr << ex.what() << std::endl;
        }

        throw;
    }

    set_env_var("TZ", orig_local_tz);
}

INSTANTIATE_TEST_SUITE_P(
    MiscellaneousTest,
    DateTime,
    ::testing::Values(
        std::make_tuple("Date", "ODBCDriver2", "Europe/Moscow",
            "toDate('2020-03-25')", SQL_TYPE_DATE,
            "2020-03-25", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 0, 0, 0, 0}
        ),
        std::make_tuple("DateTime", "ODBCDriver2", "Europe/Moscow",
            "toDateTime('2020-03-25 12:11:22')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 0}
        ),
        std::make_tuple("DateTime_TZ", "ODBCDriver2", "Europe/Moscow",
            "toDateTime('2020-03-25 12:11:22', 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 0}
        ),
        std::make_tuple("DateTime64_0", "ODBCDriver2", "Europe/Moscow",
            "toDateTime64('2020-03-25 12:11:22.123456789', 0)", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 0}
        ),
        std::make_tuple("DateTime64_4", "ODBCDriver2", "Europe/Moscow",
            "toDateTime64('2020-03-25 12:11:22.123456789', 4)", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22.1234", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 123400000}
        ),
        std::make_tuple("DateTime64_9", "ODBCDriver2", "Europe/Moscow",
            "toDateTime64('2020-03-25 12:11:22.123456789', 9)", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22.123456789", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 123456789}
        ),
        std::make_tuple("DateTime64_9_TZ", "ODBCDriver2", "Europe/Moscow",
            "toDateTime64('2020-03-25 12:11:22.123456789', 9, 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22.123456789", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 123456789}
        ),

        // TODO: remove this once the formats behave identically.

        std::make_tuple("Date", "RowBinaryWithNamesAndTypes", "Europe/Moscow",
            "toDate('2020-03-25')", SQL_TYPE_DATE,
            "2020-03-25", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 0, 0, 0, 0}
        ),
        std::make_tuple("DateTime_TZ", "RowBinaryWithNamesAndTypes", "Europe/Moscow",
            "toDateTime('2020-03-25 12:11:22', 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 09:26:22", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 9, 26, 22, 0}
        ),
        std::make_tuple("DateTime64_9_TZ", "RowBinaryWithNamesAndTypes", "Europe/Moscow",
            "toDateTime64('2020-03-25 12:11:22.123456789', 9, 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 09:26:22.123456789", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 9, 26, 22, 123456789}
        )/*,

        // TODO: uncomment once the target ClickHouse server is 21.4+

        std::make_tuple("DateTime64_9_TZ_pre_epoch", "RowBinaryWithNamesAndTypes", "Europe/Moscow",
            "toDateTime64('1955-03-25 12:11:22.123456789', 9, 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "1955-03-25 09:26:22.123456789", SQL_TIMESTAMP_STRUCT{1955, 3, 25, 9, 26, 22, 123456789}
        )
        */
    ),
    [] (const auto & param_info) {
        return std::get<0>(param_info.param) + "_over_" + std::get<1>(param_info.param);
    }
);
