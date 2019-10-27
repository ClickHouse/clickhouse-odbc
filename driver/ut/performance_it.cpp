#include "platform.h"
#include "gtest_env.h"
#include "client_utils.h"
#include "client_test_base.h"

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <sstream>

#include <cstring>

class PerformanceTest
    : public ClientTestBase
{
};

#ifdef NDEBUG

TEST_F(PerformanceTest, Basic) {
    const std::size_t total_rows_expected = 5000000;
    const std::string query = "SELECT CAST('some not very long text', 'String') as col1, CAST('12345', 'Int') as col2, CAST('12.345', 'Float32') as col3, CAST('-123.456789012345678', 'Float64') as col4 FROM numbers(" + std::to_string(total_rows_expected) + ")";
    std::cout << "Executing query:\n\t" << query << std::endl;
    const auto start = std::chrono::system_clock::now();

    std::size_t total_rows = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, (SQLTCHAR*) query.c_str(), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
    
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

        SQLCHAR col1[32] = {};
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

    const auto end = std::chrono::system_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::stringstream str;
    str << std::fixed << std::setprecision(9) << static_cast<double>(elapsed.count()) / static_cast<double>(1000'000'000);
    std::cout << "Executed in:\n\t" << str.str() << " seconds" << std::endl;
}

#endif
