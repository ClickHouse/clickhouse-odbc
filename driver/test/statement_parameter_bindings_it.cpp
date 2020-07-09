#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>

class StatementParameterBindingsTest
    : public ClientTestBase
{
};

TEST_F(StatementParameterBindingsTest, Missing) {
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

TEST_F(StatementParameterBindingsTest, NoBuffer) {
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

TEST_F(StatementParameterBindingsTest, NullStringValueForInteger) {
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

    auto param = fromUTF8<SQLmyTCHAR>("\\N");
    SQLLEN param_ind = 0;

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

TEST_F(StatementParameterBindingsTest, NullStringValueForString) {
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

    auto param = fromUTF8<SQLmyTCHAR>("\\N");
    SQLLEN param_ind = 0;

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

class StatementParameterArrayBindingsTest
    : public StatementParameterBindingsTest
    , public ::testing::WithParamInterface<std::size_t>
{
};

