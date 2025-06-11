#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <iostream>

struct DateTimeParams {
    std::string name;                            // parameter set name

    // TODO: remove this once the formats behave identically.
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

    const auto orig_local_tz = get_env_var("TZ");
    setEnvVar("TZ", params.local_tz);

#ifdef _win_
    _putenv_s("TZ", params.local_tz.data());
    _tzset();
#endif

    try {

    const std::string query_orig = "SELECT " + params.expr + " AS col FORMAT " + params.format;

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

    if (params.format != "RowBinaryWithNamesAndTypes" || params.expected_sql_type == SQL_TYPE_DATE) {
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

    if (params.format != "RowBinaryWithNamesAndTypes") {
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

    if (params.format != "RowBinaryWithNamesAndTypes" || params.expected_sql_type != SQL_TYPE_DATE) {
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
    catch (...) {
        try {
            setEnvVar("TZ", orig_local_tz);
        }
        catch (const std::exception & ex) {
            std::cerr << ex.what() << std::endl;
        }

        throw;
    }

    setEnvVar("TZ", orig_local_tz);
}

INSTANTIATE_TEST_SUITE_P(
    MiscellaneousTest,
    DateTime,
    ::testing::Values(
        DateTimeParams{"Date", "ODBCDriver2", "UTC",
            "toDate('2020-03-25')", SQL_TYPE_DATE,
            "2020-03-25", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 0, 0, 0, 0}
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
        DateTimeParams{"DateTime64_9_TZ", "ODBCDriver2", "UTC",
            "toDateTime64('2020-03-25 12:11:22.123456789', 9, 'Asia/Kathmandu')", SQL_TYPE_TIMESTAMP,
            "2020-03-25 12:11:22.123456789", SQL_TIMESTAMP_STRUCT{2020, 3, 25, 12, 11, 22, 123456789}
        },

        // TODO: remove this once the formats behave identically.

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
        }/*,

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
