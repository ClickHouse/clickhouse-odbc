#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"
#include "driver/test/date_utils.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <iostream>

struct DateTimeParams {
    std::string name;                            // parameter set name
    std::string format;                          // format to use
    std::string local_tz;                        // local timezone to use
    std::string expr;                            // expression for SELECT
    SQLSMALLINT expected_sql_type;               // expected reported column type
    std::string expected_str_val;                // value, when retrieved as string
    SQL_TIMESTAMP_STRUCT expected_timestamp_val; // value, when retrieved as SQL_TIMESTAMP_STRUCT

};

class DateTime
    : public ClientTestWithParamBase<DateTimeParams>
{
    virtual void SetUp() override {
        ClientTestWithParamBase<DateTimeParams>::SetUp();
        orig_local_tz = get_env_var("TZ");
    }

    virtual void TearDown() override {
        setEnvVar("TZ", orig_local_tz);
        ClientTestWithParamBase<DateTimeParams>::TearDown();
    }

    std::optional<std::string> orig_local_tz{};
};

TEST_P(DateTime, GetData) {
    const auto & params = GetParam();

    const SQL_DATE_STRUCT expected_date_val = {
        params.expected_timestamp_val.year,
        params.expected_timestamp_val.month,
        params.expected_timestamp_val.day
    };

    const SQL_TIME_STRUCT expected_time_val = {
        params.expected_timestamp_val.hour,
        params.expected_timestamp_val.minute,
        params.expected_timestamp_val.second
    };

    setEnvVar("TZ", params.local_tz);

#ifdef _win_
    _putenv_s("TZ", params.local_tz.data());
    _tzset();
#endif

    const std::string query_orig = "SELECT " + params.expr + " AS col FORMAT " + params.format + " SETTINGS enable_time_time64_type = 1 ";

    auto query = fromUTF8<PTChar>(query_orig);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));

    {
        SQLLEN sql_type = SQL_TYPE_NULL;
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLColAttribute(hstmt, 1, SQL_DESC_TYPE, NULL, 0, NULL, &sql_type));
        EXPECT_EQ(sql_type, params.expected_sql_type) << "expected: " << params.expected_str_val;
    }

    {
        PTChar col[64] = {};
        SQLLEN col_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(
            hstmt,
            1,
            getCTypeFor<decltype(&col[0])>(),
            &col,
            sizeof(col),
            &col_ind
        ));

        EXPECT_EQ(toUTF8(col), params.expected_str_val) << "expected: " << params.expected_str_val;;
    }

    if (params.format == "RowBinaryWithNamesAndTypes") {
        return;
    }

    if (params.expected_sql_type == SQL_TYPE_TIME || params.expected_sql_type == SQL_TYPE_TIMESTAMP) {
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

        EXPECT_EQ(col, expected_time_val) << "expected: " << params.expected_str_val;;
    }

    if (params.expected_sql_type == SQL_TYPE_DATE || params.expected_sql_type == SQL_TYPE_TIMESTAMP) {
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

        EXPECT_EQ(col, expected_date_val) << "expected: " << params.expected_str_val;;
    }

    if (params.expected_sql_type == SQL_TYPE_TIMESTAMP) {
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

        EXPECT_EQ(col, params.expected_timestamp_val) << "expected: " << params.expected_str_val;;
    }
}

INSTANTIATE_TEST_SUITE_P(
    DateTime,
    DateTime,
    ::testing::Values(
        DateTimeParams{"Time", "ODBCDriver2", "UTC",
            "CAST('15:04:05' AS Time)", SQL_TYPE_TIME,
            "15:04:05", SQL_TIMESTAMP_STRUCT{0, 0, 0, 15, 4, 5, 0}
        },
        DateTimeParams{"Time_TZ", "ODBCDriver2", "Asia/Kathmandu",
            "CAST('15:04:05' AS Time)", SQL_TYPE_TIME,
            "15:04:05", SQL_TIMESTAMP_STRUCT{0, 0, 0, 15, 4, 5, 0}
        },
        DateTimeParams{"Date", "ODBCDriver2", "UTC",
            "toDate('2020-03-25')", SQL_TYPE_DATE,
            "2020-03-25", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 0, 0, 0, 0}
        },
        DateTimeParams{"Date32", "ODBCDriver2", "UTC",
            "toDate32('2299-12-31')", SQL_TYPE_DATE,
            "2299-12-31", SQL_TIMESTAMP_STRUCT{2299, 12, 31, 0, 0, 0, 0}
        },
        DateTimeParams{"DateTime", "ODBCDriver2", "UTC",
            "toDateTime('2020-03-25 12:11:22')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 0}
        },
        DateTimeParams{"DateTime_TZ", "ODBCDriver2", "UTC",
            "toDateTime('2020-03-25 12:11:22', 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 0}
        },
        DateTimeParams{"DateTime64_0", "ODBCDriver2", "UTC",
            "toDateTime64('2020-03-25 12:11:22.123456789', 0)", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 0}
        },
        DateTimeParams{"DateTime64_4", "ODBCDriver2", "UTC",
            "toDateTime64('2020-03-25 12:11:22.123456789', 4)", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22.1234", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 123400000}
        },
        DateTimeParams{"DateTime64_9", "ODBCDriver2", "UTC",
            "toDateTime64('2020-03-25 12:11:22.123456789', 9)", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22.123456789", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 123456789}
        },
        DateTimeParams{"DateTime64_9_LeadingZeros", "ODBCDriver2", "UTC",
            "toDateTime64('2020-03-25 12:11:22.000456789', 9)", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22.000456789", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 456789}
        },
        DateTimeParams{"DateTime64_9_TZ", "ODBCDriver2", "UTC",
            "toDateTime64('2020-03-25 12:11:22.123456789', 9, 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22.123456789", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 123456789}
        },
        DateTimeParams{"Date", "RowBinaryWithNamesAndTypes", "UTC",
            "toDate('2020-03-25')", SQL_TYPE_DATE,
            "2020-03-25", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 0, 0, 0, 0}
        },
        DateTimeParams{"DateTime_TZ", "RowBinaryWithNamesAndTypes", "UTC",
            "toDateTime('2020-03-25 12:11:22', 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 06:26:22", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 6, 26, 22, 0}
        },
        DateTimeParams{"DateTime64_9_TZ", "RowBinaryWithNamesAndTypes", "UTC",
            "toDateTime64('2020-03-25 12:11:22.123456789', 9, 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 06:26:22.123456789", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 6, 26, 22, 123456789}
        }
        /*,

        // TODO: uncomment once the target ClickHouse server is 21.4+

        DateTimeParams{"DateTime64_9_TZ_pre_epoch", "RowBinaryWithNamesAndTypes", "UTC",
            "toDateTime64('1955-03-25 12:11:22.123456789', 9, 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "1955-03-25 09:26:22.123456789", SQL_TIMESTAMP_STRUCT{1955, 3, 25, 9, 26, 22, 123456789}
        }
        */
    ),
    [] (const auto & param_info) {
        return param_info.param.name + "_over_" + param_info.param.format;
    }
);

TEST_F(DateTime, DateAndDate32Compatibility)
{
    auto query = fromUTF8<PTChar>(R"""(
        SELECT date FROM (
            SELECT CAST('2006-01-02' AS Date) AS date
        )
        WHERE date = ?
    )""");
    STMT_OK(SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    SQL_DATE_STRUCT expect = {
        .year = 2006,
        .month = 1,
        .day = 2,
    };
    SQLLEN indicator = sizeof(expect);
    STMT_OK(SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_DATE, SQL_TYPE_DATE, 0, 0, &expect, 0, &indicator));
    STMT_OK(SQLExecute(hstmt));
    STMT_OK(SQLFetch(hstmt));

    SQL_DATE_STRUCT date = {0};
    indicator = 0;
    STMT_OK(SQLGetData(hstmt, 1, SQL_C_TYPE_DATE, &date, sizeof(date), &indicator));
    EXPECT_EQ(date, expect);
}

TEST_F(DateTime, TimeAsTimestamp)
{
    auto query = fromUTF8<PTChar>("SELECT CAST('15:04:05' AS Time) AS col SETTINGS enable_time_time64_type = 1");
    STMT_OK(SQLExecDirect(hstmt, ptcharCast(query.data()), SQL_NTS));
    STMT_OK(SQLFetch(hstmt));

    SQL_TIMESTAMP_STRUCT col = {};
    SQLLEN indicator = 0;
    STMT_OK(SQLGetData( hstmt, 1, SQL_C_TIMESTAMP, &col, sizeof(col), &indicator));
    SQL_TIMESTAMP_STRUCT expect = {1900, 1, 1, 15, 4, 5};
    EXPECT_EQ(col, expect);
}

TEST_F(DateTime, Time64AsTimestamp)
{
    auto query = fromUTF8<PTChar>("SELECT CAST('15:04:05.123456789' AS Time64(9)) AS col SETTINGS enable_time_time64_type = 1");
    STMT_OK(SQLExecDirect(hstmt, ptcharCast(query.data()), SQL_NTS));
    STMT_OK(SQLFetch(hstmt));

    SQLLEN sql_type = SQL_TYPE_NULL;
    STMT_OK(SQLColAttribute(hstmt, 1, SQL_DESC_TYPE, NULL, 0, NULL, &sql_type));
    // Time64 does not have corresponding ODBC data type, SQL_TYPE_TIME does not have fractions
    // and SQL_TYPE_TIMESTAMP can be misinterpreted. Here I followed what MS SQL Server does
    // and return `SQL_VARCHAR` for time with fractions of the second.
    EXPECT_EQ(sql_type, SQL_VARCHAR);

    SQL_TIMESTAMP_STRUCT col = {};
    SQLLEN indicator = 0;
    STMT_OK(SQLGetData( hstmt, 1, SQL_C_TIMESTAMP, &col, sizeof(col), &indicator));
    SQL_TIMESTAMP_STRUCT expect = {1900, 1, 1, 15, 4, 5, 123456789};
    EXPECT_EQ(col, expect);
}

TEST_F(DateTime, InsertTimeStructParam)
{
    SQL_TIME_STRUCT param = {15, 4, 5};
    SQLLEN param_ind = sizeof(SQL_TIME_STRUCT);

    auto query = fromUTF8<PTChar>("SELECT ? AS col");
    STMT_OK(SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    STMT_OK(SQLBindParameter(
        /* StatementHandle   */ hstmt,
        /* ParameterNumber   */ 1,
        /* InputOutputType   */ SQL_PARAM_INPUT,
        /* ValueType         */ SQL_C_TYPE_TIME,
        /* ParameterType     */ SQL_TYPE_TIME,
        /* ColumnSize        */ 0,
        /* DecimalDigits     */ 0,
        /* ParameterValuePtr */ &param,
        /* BufferLength      */ sizeof(param),
        /* StrLen_or_IndPtr  */ &param_ind
    ));
    STMT_OK(SQLExecute(hstmt));
    STMT_OK(SQLFetch(hstmt));

    SQL_TIME_STRUCT col = {};
    SQLLEN col_ind = 0;
    STMT_OK(SQLGetData(hstmt, 1, SQL_C_TYPE_TIME, &col, sizeof(col), &col_ind));

    const SQL_TIME_STRUCT expected = {15, 4, 5};
    EXPECT_EQ(col, expected);
}

TEST_F(DateTime, InsertTimeWithFractionThroughTimestamp)
{
    SQL_TIMESTAMP_STRUCT param = {2006, 1, 2, 15, 4, 5, 123456789};
    // NOTE: the date "2006-01-02" should be truncated
    SQLLEN param_ind = sizeof(SQL_TIME_STRUCT);

    auto create_table = fromUTF8<PTChar>(
        "CREATE OR REPLACE TABLE time ("
        "   time Time64(9)"
        ") "
        "ENGINE MergeTree "
        "ORDER BY time "
        "SETTINGS enable_time_time64_type = 1");

    STMT_OK(SQLExecDirect(hstmt, ptcharCast(create_table.data()), SQL_NTS));

    auto insert = fromUTF8<PTChar>("INSERT INTO time VALUES (?)");
    STMT_OK(SQLPrepare(hstmt, ptcharCast(insert.data()), SQL_NTS));
    STMT_OK(SQLBindParameter(
        /* StatementHandle   */ hstmt,
        /* ParameterNumber   */ 1,
        /* InputOutputType   */ SQL_PARAM_INPUT,
        /* ValueType         */ SQL_C_TYPE_TIMESTAMP,
        /* ParameterType     */ SQL_TYPE_TIMESTAMP,
        /* ColumnSize        */ 0,
        /* DecimalDigits     */ 0,
        /* ParameterValuePtr */ &param,
        /* BufferLength      */ sizeof(param),
        /* StrLen_or_IndPtr  */ &param_ind
    ));
    STMT_OK(SQLExecute(hstmt));
    ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);

    STMT_OK(SQLFreeStmt(hstmt, SQL_CLOSE));
    auto select = fromUTF8<PTChar>("SELECT time FROM time");
    STMT_OK(SQLExecDirect(hstmt, ptcharCast(select.data()), SQL_NTS));
    STMT_OK(SQLFetch(hstmt));
    SQL_TIMESTAMP_STRUCT col = {};
    SQLLEN col_ind = 0;
    STMT_OK(SQLGetData(hstmt, 1, SQL_C_TYPE_TIMESTAMP, &col, sizeof(col), &col_ind));

    const SQL_TIMESTAMP_STRUCT expected = {1900, 1, 1, 15, 4, 5, 123456789};
    EXPECT_EQ(col, expected);
}
