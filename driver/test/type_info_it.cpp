#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"
#include "driver/test/result_set_reader.hpp"

#include <format>
#include <optional>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <Poco/Foundation.h>

class TypeInfoTest
    : public ClientTestBase
{
};

// Checks that each ClickHouse type is correctly mapped to a corresponding ODBC type
TEST_F(TypeInfoTest, ClickhouseToSQLTypeMapping)
{

    struct TypeMappingTestEntry
    {
        std::string type;
        std::string input;
        SQLSMALLINT sql_type;
    };

    std::vector<TypeMappingTestEntry> types = {
        {"Bool", "0", SQL_VARCHAR},
        {"Int8", "0", SQL_TINYINT},
        {"UInt8", "0", SQL_TINYINT},
        {"Int16", "0", SQL_SMALLINT},
        {"UInt16", "0", SQL_SMALLINT},
        {"Int32", "0", SQL_INTEGER},
        {"UInt32", "0", SQL_BIGINT},
        {"Int64", "0", SQL_BIGINT},
        {"UInt64", "0", SQL_BIGINT},
        {"Int128", "0", SQL_VARCHAR},
        {"UInt128", "0", SQL_VARCHAR},
        {"Int256", "0", SQL_VARCHAR},
        {"UInt256", "0", SQL_VARCHAR},
        {"Float32", "0", SQL_REAL},
        {"Float64", "0", SQL_DOUBLE},
        {"Decimal(5)", "0", SQL_DECIMAL},
        {"Decimal32(5)", "0", SQL_DECIMAL},
        {"Decimal64(12)", "0", SQL_DECIMAL},
        {"Decimal128(24)", "0", SQL_DECIMAL},
        {"Decimal256(72)", "0", SQL_DECIMAL},
        {"String", "0", SQL_VARCHAR},
        {"FixedString(1)", "'0'", SQL_VARCHAR},
        {"Date", "0", SQL_TYPE_DATE},
        {"Date32", "0", SQL_VARCHAR},
        {"DateTime", "0", SQL_TYPE_TIMESTAMP},
        {"DateTime64", "0", SQL_TYPE_TIMESTAMP},
        {"UUID", "'00000000-0000-0000-0000-000000000000'", SQL_GUID},
        {"IPv4", "'0.0.0.0'", SQL_VARCHAR},
        {"IPv6", "'::'", SQL_VARCHAR},
        {"Array(Int32)", "[1,2,3]", SQL_VARCHAR},
        {"Tuple(Int32, Int32)", "(1,2)", SQL_VARCHAR},
        {"LowCardinality(String)", "'0'", SQL_VARCHAR},
        {"LowCardinality(Int32)", "0", SQL_INTEGER},
        {"LowCardinality(DateTime)", "0", SQL_TYPE_TIMESTAMP},
        {"Enum('hello' = 0, 'world' = 1)", "'hello'", SQL_VARCHAR},
    };

    std::unordered_map<std::string, SQLSMALLINT> sql_types{};
    std::stringstream query_stream;
    query_stream << "SELECT";
    for(const auto& [type, input, sql_type] : types) {
        auto type_escaped = Poco::replace(type, "'", "\\'");
        sql_types[std::string(type)] = sql_type;
        query_stream << std::format(" CAST({}, '{}') as `{}`,", input, type_escaped, type);
    }
    query_stream.seekp(-1, std::stringstream::cur) << " ";
    query_stream << "SETTINGS allow_suspicious_low_cardinality_types = 1";

    auto query = fromUTF8<SQLTCHAR>(query_stream.str());

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, query.data(), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

	SQLSMALLINT num_columns{};
	ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, &num_columns));

    SQLSMALLINT name_length = 0;
    SQLSMALLINT data_type = 0;

    std::basic_string<SQLTCHAR> input_name(256, '\0');
    for (SQLSMALLINT column = 1; column <= num_columns; ++column) {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLDescribeCol(
            hstmt,
            column,
            input_name.data(),
            static_cast<SQLSMALLINT>(input_name.size()),
            &name_length,
            &data_type,
            nullptr,
            nullptr,
            nullptr));
        std::string name(input_name.begin(), input_name.begin() + name_length);
        ASSERT_EQ(sql_types[name], data_type) << "type: " << name;
    }
}

TEST_F(TypeInfoTest, SQLGetTypeInfoMapping)
{
    struct SQLGetDataTypeInfoTestEntry
    {
        std::string_view column_name;
        SQLSMALLINT sql_type;
        SQLSMALLINT nullable;
    };

    // The types are taken from the SQLColumns documentation
    // https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function
    std::vector<SQLGetDataTypeInfoTestEntry> expected_columns = {
        {"TYPE_NAME", SQL_VARCHAR, false},
        {"DATA_TYPE", SQL_SMALLINT, false},
        {"COLUMN_SIZE", SQL_INTEGER, true},
        {"LITERAL_PREFIX", SQL_VARCHAR, true},
        {"LITERAL_SUFFIX", SQL_VARCHAR, true},
        {"CREATE_PARAMS", SQL_VARCHAR, true},
        {"NULLABLE", SQL_SMALLINT, false},
        {"CASE_SENSITIVE", SQL_SMALLINT, false},
        {"SEARCHABLE", SQL_SMALLINT, false},
        {"UNSIGNED_ATTRIBUTE", SQL_SMALLINT, true},
        {"FIXED_PREC_SCALE", SQL_SMALLINT, false},
        {"AUTO_UNIQUE_VALUE", SQL_SMALLINT, true},
        {"LOCAL_TYPE_NAME", SQL_VARCHAR, true},
        {"MINIMUM_SCALE", SQL_SMALLINT, true},
        {"MAXIMUM_SCALE", SQL_SMALLINT, true},
        {"SQL_DATA_TYPE", SQL_SMALLINT, false},
        {"SQL_DATETIME_SUB", SQL_SMALLINT, true},
        {"NUM_PREC_RADIX", SQL_INTEGER, true},
        {"INTERVAL_PRECISION", SQL_SMALLINT, true},
    };

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetTypeInfo(hstmt, SQL_ALL_TYPES));

    SQLSMALLINT num_columns{};
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLNumResultCols(hstmt, &num_columns));

    SQLSMALLINT name_length = 0;
    SQLSMALLINT data_type = 0;
    SQLSMALLINT nullable = 0;

    std::basic_string<SQLTCHAR> input_name(256, '\0');
    for (SQLSMALLINT column = 1; column <= num_columns; ++column) {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLDescribeCol(
            hstmt,
            column,
            input_name.data(),
            static_cast<SQLSMALLINT>(input_name.size()),
            &name_length,
            &data_type,
            nullptr,
            nullptr,
            &nullable));
        std::string name(input_name.begin(), input_name.begin() + name_length);
        ASSERT_EQ(name, expected_columns[column-1].column_name);
        ASSERT_EQ(data_type, expected_columns[column-1].sql_type) << "column " << name;
        ASSERT_EQ(nullable, expected_columns[column-1].nullable) << "column " << name;
    }
}

// Check that the SQLGetTypeInfo result is as expected
TEST_F(TypeInfoTest, SQLGetTypeInfoResultSet)
{
    struct TypeInfoEntry{
        SQLSMALLINT data_type;
        std::optional<SQLINTEGER> column_size;
        std::optional<std::string> literal_prefix;
        std::optional<std::string> literal_suffix;
        std::optional<std::string> create_params;
        std::optional<SQLINTEGER> unsigned_attribute;
        std::optional<SQLSMALLINT> minimum_scale;
        std::optional<SQLSMALLINT> maximum_scale;
        SQLSMALLINT sql_data_type;
        std::optional<SQLSMALLINT> sql_datetime_sub;
        std::optional<SQLINTEGER> num_prec_radix;
    };

    const auto na = std::nullopt;
    const int32_t max_size = 0xFFFFFF;

    // clang-format off
    std::map<std::string, TypeInfoEntry> expected {

    //                   ODBC                          pre/suffix create  unsi-  min/max                 date
    //   type name       data type           size      params     params  gned   scale    sql data type  sub  radix
        {"Nothing",     {SQL_TYPE_NULL,      1,        na,  na,   na,     na,    na, na,  SQL_TYPE_NULL, na,  na,  }},
        {"Int8",        {SQL_TINYINT,        4,        na,  na,   na,     false, na, na,  SQL_TINYINT,   na,  10,  }},
        {"UInt8",       {SQL_TINYINT,        3,        na,  na,   na,     true,  na, na,  SQL_TINYINT,   na,  10,  }},
        {"Int16",       {SQL_SMALLINT,       6,        na,  na,   na,     false, na, na,  SQL_SMALLINT,  na,  10,  }},
        {"UInt16",      {SQL_SMALLINT,       5,        na,  na,   na,     true,  na, na,  SQL_SMALLINT,  na,  10,  }},
        {"Int32",       {SQL_INTEGER,        11,       na,  na,   na,     false, na, na,  SQL_INTEGER,   na,  10,  }},
        {"UInt32",      {SQL_BIGINT,         10,       na,  na,   na,     true,  na, na,  SQL_BIGINT,    na,  10,  }},
        {"Int64",       {SQL_BIGINT,         20,       na,  na,   na,     false, na, na,  SQL_BIGINT,    na,  10,  }},
        {"UInt64",      {SQL_BIGINT,         20,       na,  na,   na,     true,  na, na,  SQL_BIGINT,    na,  10,  }},
        {"Float32",     {SQL_REAL,           7,        na,  na,   na,     false, na, na,  SQL_REAL,      na,  2,   }},
        {"Float64",     {SQL_DOUBLE,         15,       na,  na,   na,     false, na, na,  SQL_DOUBLE,    na,  2,   }},
        {"Decimal",     {SQL_DECIMAL,        41,       na,  na,   na,     false, 1,  76,  SQL_DECIMAL,   na,  10,  }},
        {"String",      {SQL_VARCHAR,        max_size, "'", "'",  na,     na,    na, na,  SQL_VARCHAR,   na,  na,  }},
        {"FixedString", {SQL_VARCHAR,        max_size, "'", "'",  na,     na,    na, na,  SQL_VARCHAR,   na,  na,  }},
        {"Date",        {SQL_TYPE_DATE,      10,       na,  na,   na,     na,    na, na,  SQL_DATE,      1,   na,  }},
        {"DateTime",    {SQL_TYPE_TIMESTAMP, 19,       na,  na,   na,     na,    na, na,  SQL_DATE,      3,   na,  }},
        {"DateTime64",  {SQL_TYPE_TIMESTAMP, 29,       na,  na,   na,     na,    na, na,  SQL_DATE,      3,   na,  }},
        {"UUID",        {SQL_GUID,           35,       na,  na,   na,     na,    na, na,  SQL_GUID,      na,  na,  }},
        {"Array",       {SQL_VARCHAR,        max_size, na,  na,   na,     na,    na, na,  SQL_VARCHAR,   na,  na,  }},
    };
    // clang-format on

    // Does not fit in the table
    expected.at("Decimal").create_params = "precision,scale";
    expected.at("FixedString").create_params = "length";

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetTypeInfo(hstmt, SQL_ALL_TYPES));

    ResultSetReader reader{hstmt};

    std::set<std::string> received_types;
    while (reader.fetch())
    {
        std::string type = reader.getData<std::string>("TYPE_NAME").value();
        SCOPED_TRACE(testing::Message() << "Failed for type " << type);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("DATA_TYPE"), expected.at(type).data_type);
        EXPECT_EQ(reader.getData<SQLINTEGER>("COLUMN_SIZE"), expected.at(type).column_size);
        EXPECT_EQ(reader.getData<std::string>("LITERAL_PREFIX"), expected.at(type).literal_prefix);
        EXPECT_EQ(reader.getData<std::string>("LITERAL_SUFFIX"), expected.at(type).literal_suffix);
        EXPECT_EQ(reader.getData<std::string>("CREATE_PARAMS"), expected.at(type).create_params);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("NULLABLE"), SQL_NULLABLE);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("CASE_SENSITIVE"), SQL_TRUE);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("SEARCHABLE"), SQL_SEARCHABLE);
        EXPECT_EQ(reader.getData<SQLINTEGER>("UNSIGNED_ATTRIBUTE"), expected.at(type).unsigned_attribute);
        EXPECT_EQ(reader.getData<SQLINTEGER>("FIXED_PREC_SCALE"), SQL_FALSE);
        EXPECT_EQ(reader.getData<SQLINTEGER>("AUTO_UNIQUE_VALUE"), std::nullopt);
        EXPECT_EQ(reader.getData<std::string>("LOCAL_TYPE_NAME"), std::nullopt);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("MINIMUM_SCALE"), expected.at(type).minimum_scale);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("MAXIMUM_SCALE"), expected.at(type).maximum_scale);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("SQL_DATA_TYPE"), expected.at(type).sql_data_type);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("SQL_DATETIME_SUB"), expected.at(type).sql_datetime_sub);
        EXPECT_EQ(reader.getData<SQLINTEGER>("NUM_PREC_RADIX"), expected.at(type).num_prec_radix);
        EXPECT_EQ(reader.getData<SQLINTEGER>("INTERVAL_PRECISION"), std::nullopt);
        received_types.insert(type);
    }

    for (const auto& [type, info] : expected) {
        EXPECT_TRUE(received_types.contains(type)) << type << " is not in SQLGetTypeInfo result set";
    }
}
