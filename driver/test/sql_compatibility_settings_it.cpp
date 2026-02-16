#include "driver/test/client_test_base.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

constexpr static bool skip_connect = true;

struct QueryResult {
    bool ok = false;
    std::string result = "";
    std::string error = "";
};

class SqlCompatibilitySettings
    : public ClientTestBase
{
public:
    explicit SqlCompatibilitySettings()
        : ClientTestBase(skip_connect) {}

    QueryResult select(std::string query)
    {
        auto query_encoded = fromUTF8<PTChar>(query);

        auto exec_res = SQLExecDirect(hstmt, ptcharCast(query_encoded.data()), SQL_NTS);
        if (exec_res != SQL_SUCCESS) {
            return QueryResult {
                .ok = false,
                .result = "",
                .error = extract_diagnostics(hstmt, SQL_HANDLE_STMT),
            };
        }

        auto fetch_res = SQLFetch(hstmt);
        if (fetch_res == SQL_NO_DATA) {
            throw std::runtime_error("unexpected NULL value");
        }
        ODBC_CALL_ON_STMT_THROW(hstmt, fetch_res);

        static constexpr size_t max_result_length = 1024;
        std::string out(max_result_length, '\0');
        SQLLEN out_len = 0;
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(
            hstmt,
            1,
            SQL_C_CHAR,
            out.data(),
            out.size(),
            &out_len
        ));
        out.resize(out_len);
        return QueryResult{.ok = true, .result = out, .error = ""};
    }
};

class SqlCompatibilitySettingsDisabled
    : public SqlCompatibilitySettings
{
public:
    explicit SqlCompatibilitySettingsDisabled()
        : SqlCompatibilitySettings() {}

    virtual void SetUp() override
    {
        ClientTestBase::SetUp();
        // Assuming the DSN has these setting disabled
        connectWithDSN(TestEnvironment::getInstance().getDSN());
    }
};

class SqlCompatibilitySettingsEnabled
    : public SqlCompatibilitySettings
{
public:
    explicit SqlCompatibilitySettingsEnabled()
        : SqlCompatibilitySettings() {}

    virtual void SetUp() override
    {
        ClientTestBase::SetUp();
        const auto & dsn = TestEnvironment::getInstance().getDSN();
        auto cs = "DSN=" + dsn + ";SQLCompatibilitySettings=1";
        connect(cs);
    }
};

const std::string cast_keep_nullable_query =
    "SELECT"
    "    SUM(CAST(number, 'Decimal')) as sum "
    "FROM ("
    "    SELECT "
    "        if((number % 2) = 0, NULL, number) AS number "
    "    FROM numbers(10)"
    ")";

const std::string prefer_column_name_to_alias_query =
    "SELECT "
    "    avg(number) AS number, "
    "    max(number) "
    "FROM numbers(1, 9);";

TEST_F(SqlCompatibilitySettingsDisabled, CastKeepNullable)
{
    auto res = select(cast_keep_nullable_query);
    ASSERT_FALSE(res.ok);
    ASSERT_TRUE(res.error.contains("Cannot convert NULL value to non-Nullable type"));
}

TEST_F(SqlCompatibilitySettingsEnabled, CastKeepNullable)
{
    auto res = select(cast_keep_nullable_query);
    ASSERT_TRUE(res.ok);
    ASSERT_EQ(res.result, "25");
}

TEST_F(SqlCompatibilitySettingsDisabled, PreferColumnNameToAlias)
{
    auto res = select(prefer_column_name_to_alias_query);
    ASSERT_FALSE(res.ok);
    ASSERT_TRUE(res.error.contains("Aggregate function avg(number) AS number is found inside another aggregate function in query"));
}

TEST_F(SqlCompatibilitySettingsEnabled, PreferColumnNameToAlias)
{
    auto res = select(prefer_column_name_to_alias_query);
    ASSERT_TRUE(res.ok);
    ASSERT_EQ(res.result, "5");
}
