#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

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

// API call that doesn't reach the driver, i.e., this is effectively (somewhat incomplete) Driver Manager overhead, just for the reference.
TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(UnimplementedAPICallOverhead)) {
    constexpr std::size_t call_count = 1'000'000;
    auto tstr = fromUTF8<PTChar>("");

    // Verify that SQLStatistics() is not implemented. Change to something else when implemented.
    {
        const auto rc = SQLStatistics(hstmt, ptcharCast(tstr.data()), 0, ptcharCast(tstr.data()), 0, ptcharCast(tstr.data()), 0, SQL_INDEX_ALL, SQL_ENSURE);
        if (rc != SQL_ERROR) {
            throw std::runtime_error(
                "SQLStatistics return code: " + std::to_string(rc) +
                ", expected SQL_ERROR (" + std::to_string(SQL_ERROR) +
                ") - a function that is not implemented by the driver"
            );
        }

        // TODO: fix this, the following extra check works reliably only on UnixODBC with matching encoding versions of driver and app.
/*
        const auto diag_str = extract_diagnostics(hstmt, SQL_HANDLE_STMT);
        ASSERT_THAT(diag_str, ::testing::HasSubstr("Driver does not support this function"));
*/
    }

    START_MEASURING_TIME();

    for (std::size_t i = 0; i < call_count; ++i) {
        SQLStatistics(hstmt, ptcharCast(tstr.data()), 0, ptcharCast(tstr.data()), 0, ptcharCast(tstr.data()), 0, SQL_INDEX_ALL, SQL_ENSURE);
    }

    STOP_MEASURING_TIME_AND_REPORT(call_count);
}

// API call that involves the driver, triggers handle dispatch and diag reset, but does no real work.
TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(NoOpAPICallOverhead)) {
    constexpr std::size_t call_count = 1'000'000;

    // Verify that several consequent SQLNumResultCols() calls  on an executed statement, without destination buffer, return SQL_SUCCESS.
    {
        auto query = fromUTF8<PTChar>("SELECT 1");

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(query.data()), SQL_NTS));
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, nullptr));
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, nullptr));
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, nullptr));
    }

    START_MEASURING_TIME();

    for (std::size_t i = 0; i < call_count; ++i) {
        SQLNumResultCols(hstmt, nullptr);
    }

    STOP_MEASURING_TIME_AND_REPORT(call_count);
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(FetchNoExtractMultiType)) {
    constexpr std::size_t total_rows_expected = 10'000'000;
    const std::string query_orig = "SELECT CAST('some not very long text', 'String') AS col1, CAST('12345', 'Int') AS col2, CAST('12.345', 'Float32') AS col3, CAST('-123.456789012345678', 'Float64') AS col4 FROM numbers(" + std::to_string(total_rows_expected) + ")";

    std::cout << "Executing query:\n\t" << query_orig << std::endl;

    auto query = fromUTF8<PTChar>(query_orig);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    std::size_t total_rows = 0;

    START_MEASURING_TIME();

    while (true) {
        const SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_NO_DATA)
            break;

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        ++total_rows;
    }

    STOP_MEASURING_TIME_AND_REPORT(total_rows);

    ASSERT_EQ(total_rows, total_rows_expected);
}


TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(FetchGetDataMultiType)) {
    constexpr std::size_t total_rows_expected = 10'000'000;
    const std::string query_orig = "SELECT CAST('some not very long text', 'String') AS col1, CAST('12345', 'Int') AS col2, CAST('12.345', 'Float32') AS col3, CAST('-123.456789012345678', 'Float64') AS col4 FROM numbers(" + std::to_string(total_rows_expected) + ")";

    std::cout << "Executing query:\n\t" << query_orig << std::endl;

    auto query = fromUTF8<PTChar>(query_orig);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    SQLTCHAR col1[32] = {};
    SQLLEN col1_ind = 0;

    SQLINTEGER col2 = 0;
    SQLLEN col2_ind = 0;

    SQLREAL col3 = 0.0;
    SQLLEN col3_ind = 0;

    SQLDOUBLE col4 = 0.0;
    SQLLEN col4_ind = 0;

    std::size_t total_rows = 0;

    START_MEASURING_TIME();

    while (true) {
        const SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_NO_DATA)
            break;

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLGetData(
                hstmt,
                1,
                getCTypeFor<decltype(&col1[0])>(),
                &col1,
                sizeof(col1),
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

    STOP_MEASURING_TIME_AND_REPORT(total_rows);

    ASSERT_EQ(total_rows, total_rows_expected);
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(FetchBindColMultiType)) {
    constexpr std::size_t total_rows_expected = 10'000'000;
    const std::string query_orig = "SELECT CAST('some not very long text', 'String') AS col1, CAST('12345', 'Int') AS col2, CAST('12.345', 'Float32') AS col3, CAST('-123.456789012345678', 'Float64') AS col4 FROM numbers(" + std::to_string(total_rows_expected) + ")";

    std::cout << "Executing query:\n\t" << query_orig << std::endl;

    auto query = fromUTF8<PTChar>(query_orig);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    SQLTCHAR col1[32] = {};
    SQLLEN col1_ind = 0;

    SQLINTEGER col2 = 0;
    SQLLEN col2_ind = 0;

    SQLREAL col3 = 0.0;
    SQLLEN col3_ind = 0;

    SQLDOUBLE col4 = 0.0;
    SQLLEN col4_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindCol(
            hstmt,
            1,
            getCTypeFor<decltype(&col1[0])>(),
            &col1,
            sizeof(col1),
            &col1_ind
        )
    );

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindCol(
            hstmt,
            2,
            getCTypeFor<decltype(col2)>(),
            &col2,
            sizeof(col2),
            &col2_ind
        )
    );

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindCol(
            hstmt,
            3,
            getCTypeFor<decltype(col3)>(),
            &col3,
            sizeof(col3),
            &col3_ind
        )
    );

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindCol(
            hstmt,
            4,
            getCTypeFor<decltype(col4)>(),
            &col4,
            sizeof(col4),
            &col4_ind
        )
    );

    std::size_t total_rows = 0;

    START_MEASURING_TIME();

    while (true) {
        const SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_NO_DATA)
            break;

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        ++total_rows;
    }

    STOP_MEASURING_TIME_AND_REPORT(total_rows);

    ASSERT_EQ(total_rows, total_rows_expected);
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(FetchBindColSingleType_ANSI_String)) {
    constexpr std::size_t total_rows_expected = 10'000'000;
    const std::string query_orig = "SELECT CAST('some not very long text', 'String') AS col FROM numbers(" + std::to_string(total_rows_expected) + ")";

    std::cout << "Executing query:\n\t" << query_orig << std::endl;

    auto query = fromUTF8<PTChar>(query_orig);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    SQLCHAR col[32] = {};
    SQLLEN col_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindCol(
            hstmt,
            1,
            getCTypeFor<decltype(&col[0])>(),
            &col,
            sizeof(col),
            &col_ind
        )
    );

    std::size_t total_rows = 0;

    START_MEASURING_TIME();

    while (true) {
        const SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_NO_DATA)
            break;

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        ++total_rows;
    }

    STOP_MEASURING_TIME_AND_REPORT(total_rows);

    ASSERT_EQ(total_rows, total_rows_expected);
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(FetchBindColSingleType_Unicode_String)) {
    constexpr std::size_t total_rows_expected = 10'000'000;
    const std::string query_orig = "SELECT CAST('some not very long text', 'String') AS col FROM numbers(" + std::to_string(total_rows_expected) + ")";

    std::cout << "Executing query:\n\t" << query_orig << std::endl;

    auto query = fromUTF8<PTChar>(query_orig);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    SQLWCHAR col[32] = {};
    SQLLEN col_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindCol(
            hstmt,
            1,
            getCTypeFor<decltype(&col[0])>(),
            &col,
            sizeof(col),
            &col_ind
        )
    );

    std::size_t total_rows = 0;

    START_MEASURING_TIME();

    while (true) {
        const SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_NO_DATA)
            break;

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        ++total_rows;
    }

    STOP_MEASURING_TIME_AND_REPORT(total_rows);

    ASSERT_EQ(total_rows, total_rows_expected);
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(FetchBindColSingleType_Int)) {
    constexpr std::size_t total_rows_expected = 10'000'000;
    const std::string query_orig = "SELECT CAST('12345', 'Int') AS col FROM numbers(" + std::to_string(total_rows_expected) + ")";

    std::cout << "Executing query:\n\t" << query_orig << std::endl;

    auto query = fromUTF8<PTChar>(query_orig);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    SQLINTEGER col = 0;
    SQLLEN col_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindCol(
            hstmt,
            1,
            getCTypeFor<decltype(col)>(),
            &col,
            sizeof(col),
            &col_ind
        )
    );

    std::size_t total_rows = 0;

    START_MEASURING_TIME();

    while (true) {
        const SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_NO_DATA)
            break;

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        ++total_rows;
    }

    STOP_MEASURING_TIME_AND_REPORT(total_rows);

    ASSERT_EQ(total_rows, total_rows_expected);
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(FetchBindColSingleType_Float64)) {
    constexpr std::size_t total_rows_expected = 10'000'000;
    const std::string query_orig = "SELECT CAST('-123.456789012345678', 'Float64') AS col FROM numbers(" + std::to_string(total_rows_expected) + ")";

    std::cout << "Executing query:\n\t" << query_orig << std::endl;

    auto query = fromUTF8<PTChar>(query_orig);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    SQLDOUBLE col = 0.0;
    SQLLEN col_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindCol(
            hstmt,
            1,
            getCTypeFor<decltype(col)>(),
            &col,
            sizeof(col),
            &col_ind
        )
    );

    std::size_t total_rows = 0;

    START_MEASURING_TIME();

    while (true) {
        const SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_NO_DATA)
            break;

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        ++total_rows;
    }

    STOP_MEASURING_TIME_AND_REPORT(total_rows);

    ASSERT_EQ(total_rows, total_rows_expected);
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(FetchArrayBindColSingleType_Int)) {
    constexpr std::size_t total_rows_expected = 10'000'000;
    const std::string query_orig = "SELECT CAST('12345', 'Int') AS col FROM numbers(" + std::to_string(total_rows_expected) + ")";

    std::cout << "Executing query:\n\t" << query_orig << std::endl;

    auto query = fromUTF8<PTChar>(query_orig);

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    SQLULEN rows_processed = 0;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR, (SQLPOINTER)&rows_processed, 0));

    const std::size_t array_size = 1'000;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)array_size, 0));

    SQLINTEGER col[array_size] = {};
    SQLLEN col_ind[array_size] = {};

    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindCol(
            hstmt,
            1,
            getCTypeFor<std::decay_t<decltype(*col)>>(),
            col,
            sizeof(*col),
            col_ind
        )
    );

    std::size_t total_rows = 0;

    START_MEASURING_TIME();

    while (true) {
        const SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_NO_DATA)
            break;

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        total_rows += rows_processed;
    }

    STOP_MEASURING_TIME_AND_REPORT(total_rows);

    ASSERT_EQ(total_rows, total_rows_expected);
}
