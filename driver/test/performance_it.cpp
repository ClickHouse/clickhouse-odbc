#include "driver/platform/platform.h"
#include "driver/test/gtest_env.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <sstream>

#include <cstring>

class PerformanceTest
    : public ClientTestBase
{
protected:
    virtual void SetUp() override {
        ClientTestBase::SetUp();

#if !defined(_IODBCUNIX_H)
        ODBC_CALL_ON_DBC_THROW(hdbc, SQLGetConnectAttr(hdbc, SQL_ATTR_TRACE, &driver_manager_trace, 0, nullptr));
        if (driver_manager_trace == SQL_OPT_TRACE_ON) {
            std::cout << "Temporarily disabling driver manager tracing..." << std::endl;
            ODBC_CALL_ON_DBC_THROW(hdbc, SQLSetConnectAttr(hdbc, SQL_ATTR_TRACE, (SQLPOINTER)SQL_OPT_TRACE_OFF, 0));
        }
#else
        std::cout << "WARNING: using iODBC, so unable to detect and disable driver manager tracing from the client code!" <<
            " Performance test results may not be very representative and may take significantly more time and disk space." << std::endl;
#endif

        ODBC_CALL_ON_DBC_THROW(hdbc, SQLGetConnectAttr(hdbc, CH_SQL_ATTR_DRIVERLOG, &driver_log, 0, nullptr));
        if (driver_log == SQL_OPT_TRACE_ON) {
            std::cout << "Temporarily disabling driver logging..." << std::endl;
            ODBC_CALL_ON_DBC_THROW(hdbc, SQLSetConnectAttr(hdbc, CH_SQL_ATTR_DRIVERLOG, (SQLPOINTER)SQL_OPT_TRACE_OFF, 0));
        }
    }

    virtual void TearDown() override {
        if (driver_log == SQL_OPT_TRACE_ON) {
            std::cout << "Re-enabling driver logging..." << std::endl;
            ODBC_CALL_ON_DBC_THROW(hdbc, SQLSetConnectAttr(hdbc, CH_SQL_ATTR_DRIVERLOG, (SQLPOINTER)SQL_OPT_TRACE_ON, 0));
        }

#if !defined(_IODBCUNIX_H)
        if (driver_manager_trace == SQL_OPT_TRACE_ON) {
            std::cout << "Re-enabling driver manager tracing..." << std::endl;
            ODBC_CALL_ON_DBC_THROW(hdbc, SQLSetConnectAttr(hdbc, SQL_ATTR_TRACE, (SQLPOINTER)SQL_OPT_TRACE_ON, 0));
        }
#endif

        ClientTestBase::TearDown();
    }

private:
#if !defined(_IODBCUNIX_H)
    SQLUINTEGER driver_manager_trace = SQL_OPT_TRACE_ON;
#endif
    SQLUINTEGER driver_log = SQL_OPT_TRACE_ON;
};

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(Basic)) {
    constexpr std::size_t total_rows_expected = 1'000'000;
    const std::string query_orig = "SELECT CAST('some not very long text', 'String') as col1, CAST('12345', 'Int') as col2, CAST('12.345', 'Float32') as col3, CAST('-123.456789012345678', 'Float64') as col4 FROM numbers(" + std::to_string(total_rows_expected) + ")";

    std::cout << "Executing query:\n\t" << query_orig << std::endl;

    const auto query = fromUTF8<SQLTCHAR>(query_orig);
    auto * query_wptr = const_cast<SQLTCHAR * >(query.c_str());

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query_wptr, SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    START_MEASURING_TIME();

    std::size_t total_rows = 0;

    while (true) {
        SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_NO_DATA)
            break;

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        SQLTCHAR col1[32] = {};
        SQLLEN col1_ind = 0;

        SQLINTEGER col2 = 0;
        SQLLEN col2_ind = 0;

        SQLREAL col3 = 0.0;
        SQLLEN col3_ind = 0;

        SQLDOUBLE col4 = 0.0;
        SQLLEN col4_ind = 0;

        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLGetData(
                hstmt,
                1,
                getCTypeFor<decltype(&col1[0])>(),
                &col1,
                lengthof(col1),
                &col1_ind
            )
        );

        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLGetData(
                hstmt,
                2,
                getCTypeFor<decltype(col2)>(),
                &col2,
                sizeof(col2),
                &col2_ind
            )
        );

        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLGetData(
                hstmt,
                3,
                getCTypeFor<decltype(col3)>(),
                &col3,
                sizeof(col3),
                &col3_ind
            )
        );

        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLGetData(
                hstmt,
                4,
                getCTypeFor<decltype(col4)>(),
                &col4,
                sizeof(col4),
                &col4_ind
            )
        );

        ++total_rows;
    }

    ASSERT_EQ(total_rows, total_rows_expected);

    STOP_MEASURING_TIME_AND_REPORT();
}
