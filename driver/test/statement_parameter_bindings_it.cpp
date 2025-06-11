#include "driver/platform/platform.h"
#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>

class StatementParameterBindingsTest
    : public ClientTestBase
{
};

TEST_F(StatementParameterBindingsTest, Missing) {
    auto query = fromUTF8<PTChar>("SELECT isNull(?)");

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
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
    auto query = fromUTF8<PTChar>("SELECT isNull(?)");

    SQLINTEGER param = 0;
    SQLLEN param_ind = 0;

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
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

TEST_F(StatementParameterBindingsTest, NullValueForInteger) {
    auto query = fromUTF8<PTChar>("SELECT isNull(?)");

    SQLLEN param_ind = SQL_NULL_DATA;

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindParameter(
            hstmt,
            1,
            SQL_PARAM_INPUT,
            SQL_C_TCHAR,
            SQL_INTEGER,
            0,
            0,
            nullptr,
            0,
            &param_ind
        )
    );

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));

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

TEST_F(StatementParameterBindingsTest, NullValueForString) {
    auto query = fromUTF8<PTChar>("SELECT isNull(?)");

    SQLLEN param_ind = SQL_NULL_DATA;

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindParameter(
            hstmt,
            1,
            SQL_PARAM_INPUT,
            SQL_C_WCHAR,
            SQL_VARCHAR,
            0,
            0,
            nullptr,
            0,
            &param_ind
        )
    );

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));

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

TEST_F(StatementParameterBindingsTest, IntArray) {
    auto query = fromUTF8<PTChar>("SELECT ?");

    SQLINTEGER param[] = { 1, 2, 3 };
    SQLLEN param_ind[] = { 0, 0, 0 };

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER)lengthof(param), 0));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindParameter(
            hstmt,
            1,
            SQL_PARAM_INPUT,
            getCTypeFor<std::decay_t<decltype(param[0])>>(),
            SQL_INTEGER,
            0,
            0,
            param,
            0,
            param_ind
        )
    );
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    for (std::size_t i = 0; i < lengthof(param); ++i) {
        SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        SQLINTEGER col = 0;
        SQLLEN col_ind = -1;

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
        ASSERT_EQ(col, param[i]);

        ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
        ASSERT_EQ(SQLMoreResults(hstmt), (i + 1 == lengthof(param) ? SQL_NO_DATA : SQL_SUCCESS));
    }
}

TEST_F(StatementParameterBindingsTest, StringArray) {
    auto query = fromUTF8<PTChar>("SELECT ?");

    SQLCHAR param[][10] = { "aaa", "bbbb", "ccccc" };
    SQLLEN param_ind[] = { SQL_NTS, 4, SQL_NTS };

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER)lengthof(param), 0));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt,
        SQLBindParameter(
            hstmt,
            1,
            SQL_PARAM_INPUT,
            getCTypeFor<SQLCHAR *>(),
            SQL_CHAR,
            0,
            0,
            param,
            lengthof(param[0]),
            param_ind
        )
    );
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    for (std::size_t i = 0; i < lengthof(param); ++i) {
        SQLRETURN rc = SQLFetch(hstmt);

        if (rc == SQL_ERROR)
            throw std::runtime_error(extract_diagnostics(hstmt, SQL_HANDLE_STMT));

        if (rc == SQL_SUCCESS_WITH_INFO)
            std::cout << extract_diagnostics(hstmt, SQL_HANDLE_STMT) << std::endl;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("SQLFetch return code: " + std::to_string(rc));

        SQLCHAR col[8] = {};
        SQLLEN col_ind = -1;

        ODBC_CALL_ON_STMT_THROW(hstmt,
            SQLGetData(
                hstmt,
                1,
                getCTypeFor<SQLCHAR *>(),
                &col,
                sizeof(col),
                &col_ind
            )
        );

        ASSERT_TRUE(col_ind >= 0 || col_ind == SQL_NTS);
        ASSERT_STREQ((char *)col, (char *)param[i]);

        ASSERT_EQ(SQLFetch(hstmt), SQL_NO_DATA);
        ASSERT_EQ(SQLMoreResults(hstmt), (i + 1 == lengthof(param) ? SQL_NO_DATA : SQL_SUCCESS));
    }
}
