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

TEST_F(StatementParameterBindingsTest, DISABLED_UTFCharacters)
{
    // This test is disabled.
    // The driver manager has to translate from UTF-8 to UTF-16, which it does not do correctly, or
    // the way it does it depends on the system configuration (as it is done on Windows). In
    // general, it does not work and is not recommended. If users want to pass UTF-8 strings, they
    // should use the ANSI version of the driver, and for UTF-16 they should use the Unicode version
    // of the driver. It would make much more sense to run this test only when the driver and
    // application encodings match. However, we do not have a tool to filter tests in that way, so
    // the test is disabled for now.
    auto create_query = fromUTF8<PTChar>(
        "CREATE OR REPLACE TABLE strings (string String) engine MergeTree order by string");
    STMT_OK(SQLExecDirect(hstmt, ptcharCast(create_query.data()), SQL_NTS));
    STMT_OK(SQLFreeStmt(hstmt, SQL_CLOSE));

    const char string[] = "Hello こんにちは Привет 🌍 😜";
    auto insert_query = fromUTF8<PTChar>(
        "INSERT INTO strings VALUES (?), ('" + std::string(string) + "')");
    auto param = fromUTF8<PTChar>(string);
    STMT_OK(SQLPrepare(hstmt, ptcharCast(insert_query.data()), SQL_NTS));
    SQLLEN param_length = param.size() * sizeof(PTChar);
    STMT_OK(SQLBindParameter(
        hstmt, 1,
        SQL_PARAM_INPUT,
        getCTypeFor<decltype(param.data())>(),
        SQL_VARCHAR,
        1024,
        0,
        param.data(),
        param_length,
        &param_length));
    STMT_OK(SQLExecute(hstmt));
    STMT_OK(SQLFreeStmt(hstmt, SQL_CLOSE));

    auto select_query = fromUTF8<PTChar>("SELECT string FROM strings");
    char buffer[1024] = {0};
    SQLLEN bytes_read = 0;
    STMT_OK(SQLExecDirect(hstmt, ptcharCast(select_query.data()), SQL_NTS));

    STMT_OK(SQLFetch(hstmt));
    STMT_OK(SQLGetData(hstmt, 1, SQL_C_BINARY, buffer, sizeof(buffer), &bytes_read));
    ASSERT_EQ(bytes_read, std::size(string) - 1);
    ASSERT_TRUE(std::equal(std::begin(buffer), std::begin(buffer) + bytes_read, std::begin(string)));

    STMT_OK(SQLFetch(hstmt));
    STMT_OK(SQLGetData(hstmt, 1, SQL_C_BINARY, buffer, sizeof(buffer), &bytes_read));
    ASSERT_EQ(bytes_read, std::size(string) - 1);
    ASSERT_TRUE(std::equal(std::begin(buffer), std::begin(buffer) + bytes_read, std::begin(string)));

    STMT_OK(SQLFreeStmt(hstmt, SQL_CLOSE));
}

class StringParameterBindingTest
    : public ClientTestWithParamBase<std::string>
{
public:
    using Base = ClientTestWithParamBase<std::string>;
    explicit StringParameterBindingTest()
        : Base() {}
    virtual void SetUp() override {
        Base::SetUp();
        auto create_query = fromUTF8<PTChar>(
            "CREATE OR REPLACE TABLE strings (string String) engine MergeTree order by string");
        STMT_OK(SQLExecDirect(hstmt, ptcharCast(create_query.data()), SQL_NTS));
        STMT_OK(SQLFreeStmt(hstmt, SQL_CLOSE));
    }
};

// The list of special characters is fetched from https://clickhouse.com/docs/sql-reference/syntax#string
INSTANTIATE_TEST_SUITE_P(StringParameterBindingTest, StringParameterBindingTest, ::testing::Values(
    "foo \\",
    "foo \\\\",
    "foo \\N bar",
    "foo \a bar",
    "foo \b bar",
    "foo \e bar",
    "foo \f bar",
    "foo \n bar",
    "foo \r bar",
    "foo \t bar",
    "foo \\t bar",   // `\` followed by t
    "foo \\\t bar",  // `\\` followed by a tab
    "foo \v bar",
    std::string("foo \0 bar", 9),
    "foo \\ bar",
    "foo ' bar",
    "foo \" bar",
    "foo ` bar",
    "foo / bar",
    "foo = bar",
    "foo \x01 bar",
    "foo \x02 bar"
));

TEST_P(StringParameterBindingTest, SpecialCharactersRoundTrip)
{
    auto str  = GetParam();

    auto insert_query = fromUTF8<PTChar>("INSERT INTO strings VALUES (?)");
    auto param = fromUTF8<PTChar>(str);
    STMT_OK(SQLPrepare(hstmt, ptcharCast(insert_query.data()), SQL_NTS));
    SQLLEN param_length = param.size() * sizeof(PTChar);
    STMT_OK(SQLBindParameter(
        hstmt, 1,
        SQL_PARAM_INPUT,
        getCTypeFor<decltype(param.data())>(),
        SQL_VARCHAR,
        1024,
        0,
        param.data(),
        param_length,
        &param_length));
    STMT_OK(SQLExecute(hstmt));
    STMT_OK(SQLFreeStmt(hstmt, SQL_CLOSE));

    auto select_query = fromUTF8<PTChar>("SELECT string FROM strings");
    char buffer[1024] = {0};
    SQLLEN bytes_read = 0;
    STMT_OK(SQLExecDirect(hstmt, ptcharCast(select_query.data()), SQL_NTS));

    STMT_OK(SQLFetch(hstmt));
    STMT_OK(SQLGetData(hstmt, 1, SQL_C_BINARY, buffer, sizeof(buffer), &bytes_read));
    ASSERT_EQ(bytes_read, str.size());
    ASSERT_TRUE(std::equal(std::begin(buffer), std::begin(buffer) + bytes_read, str.begin()));

    STMT_OK(SQLFreeStmt(hstmt, SQL_CLOSE));
}
