#include "driver/test/client_utils.h"
#include "driver/test/client_test_base.h"
#include "driver/test/result_set_reader.hpp"

#include <format>
#include <optional>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <Poco/Foundation.h>

static std::string database_name = "default";

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

// Checks that the SQLGetTypeInfo function returns a dataset
// with column types as specified in the ODBC documentation.
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

// Check that the SQLGetTypeInfo returns a dataset with the data as expected
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

    const std::string pre_sc = "precision,scale";
    const std::string length = "length";
    const std::string scale = "scale";

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
        {"Decimal",     {SQL_DECIMAL,        41,       na,  na,   pre_sc, false, 1,  76,  SQL_DECIMAL,   na,  10,  }},
        {"String",      {SQL_VARCHAR,        max_size, "'", "'",  na,     na,    na, na,  SQL_VARCHAR,   na,  na,  }},
        {"FixedString", {SQL_VARCHAR,        max_size, "'", "'",  length, na,    na, na,  SQL_VARCHAR,   na,  na,  }},
        {"Date",        {SQL_TYPE_DATE,      10,       na,  na,   na,     na,    na, na,  SQL_DATE,      1,   na,  }},
        {"DateTime64",  {SQL_TYPE_TIMESTAMP, 29,       na,  na,   scale,  na,    0,  9,   SQL_DATE,      3,   na,  }},
        {"DateTime",    {SQL_TYPE_TIMESTAMP, 19,       na,  na,   na,     na,    na, na,  SQL_DATE,      3,   na,  }},
        {"UUID",        {SQL_GUID,           35,       na,  na,   na,     na,    na, na,  SQL_GUID,      na,  na,  }},
        {"Array",       {SQL_VARCHAR,        max_size, na,  na,   na,     na,    na, na,  SQL_VARCHAR,   na,  na,  }},
    };
    // clang-format on

    expected["Nullable(Decimal)"] = expected["Decimal"];

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

// Check the ClickHouse types associated by the driver with each of the SQL
// types, i.e. SQL_VARCHAR is associated with String, SQL_BIGINT with Int64,
// etc.
TEST_F(TypeInfoTest, SQLToClickHouseAssociatedTypes)
{
    using enum DataSourceTypeId;
    std::unordered_map<SQLSMALLINT, std::string> type_map {
        {SQL_TINYINT, "Int8"},
        {SQL_SMALLINT, "Int16"},
        {SQL_INTEGER, "Int32"},
        {SQL_BIGINT, "Int64"},
        {SQL_REAL, "Float32"},
        {SQL_DOUBLE, "Float64"},
        {SQL_DECIMAL, "Nullable(Decimal)"},
        {SQL_TYPE_DATE, "Date"},
        {SQL_TYPE_TIMESTAMP, "DateTime64"},
        {SQL_GUID, "UUID"},
        {SQL_VARCHAR, "String"}
    };

    for (const auto& [sql_type, ch_type] : type_map)
    {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetTypeInfo(hstmt, sql_type));
        ResultSetReader reader{hstmt};
        reader.fetch();
        EXPECT_EQ(reader.getData<std::string>("TYPE_NAME").value(), ch_type) << "for type " << sql_type;
        SQLFreeStmt(hstmt, SQL_CLOSE);
    }
}

// Checks that the SQLColumns function returns a dataset
// with column types as specified in the ODBC documentation.
TEST_F(TypeInfoTest, SQLColumnTypeMapping)
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
        {"TABLE_CAT", SQL_VARCHAR, true},
        {"TABLE_SCHEM", SQL_VARCHAR, true},
        {"TABLE_NAME", SQL_VARCHAR, false},
        {"COLUMN_NAME", SQL_VARCHAR, false},
        {"DATA_TYPE", SQL_SMALLINT, false},
        {"TYPE_NAME", SQL_VARCHAR, false},
        {"COLUMN_SIZE", SQL_INTEGER, true},
        {"BUFFER_LENGTH", SQL_INTEGER, true},
        {"DECIMAL_DIGITS", SQL_SMALLINT, true},
        {"NUM_PREC_RADIX", SQL_SMALLINT, true},
        {"NULLABLE", SQL_SMALLINT, false},
        {"REMARKS", SQL_VARCHAR, true},
        {"COLUMN_DEF", SQL_VARCHAR, true},
        {"SQL_DATA_TYPE", SQL_SMALLINT, false},
        {"SQL_DATETIME_SUB", SQL_SMALLINT, true},
        {"CHAR_OCTET_LENGTH", SQL_INTEGER, true},
        {"ORDINAL_POSITION", SQL_INTEGER, false},
        {"IS_NULLABLE", SQL_VARCHAR, true},
    };

    auto catalog_name = fromUTF8<SQLTCHAR>("system");
    auto table_name = fromUTF8<SQLTCHAR>("databases");

    ODBC_CALL_ON_STMT_THROW(hstmt, SQLColumns(
        hstmt,
        catalog_name.data(), catalog_name.size(),
        (SQLTCHAR*)"", 0,
        table_name.data(), table_name.size(),
        nullptr, 0));

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

// Check that the SQLColumns returns a dataset with the data as expected for each type of the column
TEST_F(TypeInfoTest, AllTypesColumns)
{
    static std::string table_name = "all_types_columns";

    struct SQLColumnsEntry
    {
        SQLSMALLINT data_type;
        std::string type_name;
        std::optional<SQLINTEGER> column_size;
        std::optional<SQLINTEGER> buffer_length;
        std::optional<SQLSMALLINT> decimal_digits;
        std::optional<SQLSMALLINT> num_prec_radix;
        SQLSMALLINT nullable;
        SQLSMALLINT sql_data_type;
        std::optional<SQLSMALLINT> sql_datetime_sub;
        std::optional<SQLINTEGER> char_octet_length;
        SQLINTEGER ordinal_position{};
    };

    const int32_t msz = 0xFFFFFF;
    auto na = std::nullopt;
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // clang-format off
    std::map<std::string, SQLColumnsEntry> expected {
    //                                                    col  buf  dec                               date oct
    // CH type name     ODBC type          type           size len  dig   radix  null   SQL type      sub  len
    {"Int8",           {SQL_TINYINT,       "Int8",        4,   0,   na,   10,    false, SQL_TINYINT,  na,  1   }},
    {"UInt8",          {SQL_TINYINT,       "UInt8",       3,   0,   na,   10,    false, SQL_TINYINT,  na,  1   }},
    {"Int16",          {SQL_SMALLINT,      "Int16",       6,   0,   na,   10,    false, SQL_SMALLINT, na,  2   }},
    {"UInt16",         {SQL_SMALLINT,      "UInt16",      5,   0,   na,   10,    false, SQL_SMALLINT, na,  2   }},
    {"Int32",          {SQL_INTEGER,       "Int32",       11,  0,   na,   10,    false, SQL_INTEGER,  na,  4   }},
    {"UInt32",         {SQL_BIGINT,        "UInt32",      10,  0,   na,   10,    false, SQL_BIGINT,   na,  4   }},
    {"Int64",          {SQL_BIGINT,        "Int64",       20,  0,   na,   10,    false, SQL_BIGINT,   na,  8   }},
    {"UInt64",         {SQL_BIGINT,        "UInt64",      20,  0,   na,   10,    false, SQL_BIGINT,   na,  8   }},
    {"Float32",        {SQL_REAL,          "Float32",     7,   0,   na,   2,     false, SQL_REAL,     na,  4   }},
    {"Float64",        {SQL_DOUBLE,        "Float64",     15,  0,   na,   2,     false, SQL_DOUBLE,   na,  8   }},
    {"Decimal",        {SQL_DECIMAL,       "Decimal",     10,  0,   0,    10,    false, SQL_DECIMAL,  na,  32  }},
    {"Decimal(2)",     {SQL_DECIMAL,       "Decimal",     2,   0,   0,    10,    false, SQL_DECIMAL,  na,  32  }},
    {"Decimal(12,5)",  {SQL_DECIMAL,       "Decimal",     12,  0,   5,    10,    false, SQL_DECIMAL,  na,  32  }},
    {"String",         {SQL_VARCHAR,       "String",      msz, 0,   na,   na,    false, SQL_VARCHAR,  na,  msz }},
    {"FixedString(42)",{SQL_VARCHAR,       "FixedString", 42,  0,   na,   na,    false, SQL_VARCHAR,  na,  msz }},
    {"Date",           {SQL_TYPE_DATE,     "Date",        10,  0,   na,   na,    false, SQL_DATE,     1,   6   }},
    {"DateTime",       {SQL_TYPE_TIMESTAMP,"DateTime",    19,  0,   0,    na,    false, SQL_DATE,     3,   16  }},
    {"DateTime64",     {SQL_TYPE_TIMESTAMP,"DateTime64",  29,  0,   3,    na,    false, SQL_DATE,     3,   16  }},
    {"DateTime64(9)",  {SQL_TYPE_TIMESTAMP,"DateTime64",  29,  0,   9,    na,    false, SQL_DATE,     3,   16  }},
    {"UUID",           {SQL_GUID,          "UUID",        35,  0,   na,   na,    false, SQL_GUID,     na,  16  }},
    // Bool is currently converted to a string ("true" or "false")
    {"Bool",           {SQL_VARCHAR,       "String",      msz, 0,   na,   na,    false, SQL_VARCHAR,  na,  msz }},
    };
    // clang-format on

    auto expected_copy = expected;
    // Add nullable for each type
    for (auto [type, value] : expected_copy) {
        value.nullable = true;
        expected.insert({"Nullable(" + type + ")", value});
    }

    // Types that cannot be nullable or require special treatment
    // clang-format off
    expected.insert({
    //                                    col  buf  dec                               date oct
    //  ODBC type          type           size len  dig   radix  null   SQL type      sub  len
    {"Array(UInt8)",   {
        SQL_VARCHAR,       "String",      msz, 0,   na,   na,    false, SQL_VARCHAR,  na,  msz }},
    {"LowCardinality(String)", {
        SQL_VARCHAR,       "String",      msz, 0,   na,   na,    false, SQL_VARCHAR,  na,  msz }},
    // TODO(slabko): Low cardinality nullable types do not detect nullability correctly
    // Notice `null` is set to `false` here, but must be `true`
    {"LowCardinality(Nullable(String))", {
        SQL_VARCHAR,       "String",      msz, 0,   na,   na,    false,  SQL_VARCHAR,  na,  msz }},
    });
    // clang-format on

    size_t pos = 1;
    std::stringstream query_stream;
    query_stream << "create or replace table " << database_name << "." << table_name << "(\n";
    for (auto& [type, entry] : expected) {
        query_stream << " `" << type << "` " << type << ",\n";
        entry.ordinal_position = pos++;
    }

    query_stream.seekp(-1, std::stringstream::cur) << " ";
    query_stream << ") engine MergeTree order by Int8";

    auto query = fromUTF8<SQLTCHAR>(query_stream.str());
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, query.data(), SQL_NTS));
    auto database = fromUTF8<SQLTCHAR>(database_name);
    auto table = fromUTF8<SQLTCHAR>(table_name);
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLColumns(
        hstmt,
        database.data(), database.size(),
        (SQLTCHAR*)"", 0,
        table.data(), table.size(),
        nullptr, 0));

    ResultSetReader reader{hstmt};

    std::set<std::string> received_columns;
    while (reader.fetch())
    {
        std::string column = reader.getData<std::string>("COLUMN_NAME").value();
        SCOPED_TRACE(testing::Message() << "Failed for column " << column);

        EXPECT_EQ(reader.getData<std::string>("TABLE_CAT"), database_name);
        EXPECT_EQ(reader.getData<std::string>("TABLE_SCHEM"), "");
        EXPECT_EQ(reader.getData<std::string>("TABLE_NAME"), table_name);
        EXPECT_EQ(reader.getData<std::string>("COLUMN_NAME"), column);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("DATA_TYPE"), expected.at(column).data_type);
        EXPECT_EQ(reader.getData<std::string>("TYPE_NAME"), expected.at(column).type_name);
        EXPECT_EQ(reader.getData<SQLINTEGER>("COLUMN_SIZE"), expected.at(column).column_size);
        EXPECT_EQ(reader.getData<SQLINTEGER>("BUFFER_LENGTH"), expected.at(column).buffer_length);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("DECIMAL_DIGITS"), expected.at(column).decimal_digits);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("NUM_PREC_RADIX"), expected.at(column).num_prec_radix);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("NULLABLE"), expected.at(column).nullable ? SQL_NULLABLE : SQL_NO_NULLS);
        EXPECT_EQ(reader.getData<std::string>("REMARKS"), na);
        EXPECT_EQ(reader.getData<std::string>("COLUMN_DEF"), na);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("SQL_DATA_TYPE"), expected.at(column).sql_data_type);
        EXPECT_EQ(reader.getData<SQLSMALLINT>("SQL_DATETIME_SUB"), expected.at(column).sql_datetime_sub);
        EXPECT_EQ(reader.getData<SQLINTEGER>("CHAR_OCTET_LENGTH"), expected.at(column).char_octet_length);
        EXPECT_EQ(reader.getData<SQLINTEGER>("ORDINAL_POSITION"), expected.at(column).ordinal_position);
        EXPECT_EQ(reader.getData<std::string>("IS_NULLABLE"), expected.at(column).nullable ? "YES" : "NO");
        received_columns.insert(column);
    }

    for (const auto& [column, info] : expected) {
        EXPECT_TRUE(received_columns.contains(column)) << column << " is not in SQLColumns result set";
    }
}

// Inserts DateTime and DateTime64 values into a table and checks that
// it can filter by these values and the resulting values are exactly
// the same as the inserted values
TEST_F(TypeInfoTest, TimestampTypes)
{
    static std::string table_name = "datetime_test";
    SQL_TIMESTAMP_STRUCT datetime   {2025, 4, 15, 14, 45, 40, 0};
    SQL_TIMESTAMP_STRUCT datetime64 {2025, 4, 15, 14, 45, 40, 123456789};

    auto create_table_query = fromUTF8<SQLTCHAR>(std::format(R"(
        CREATE OR REPLACE TABLE {}.{} (
            dt DateTime,
            dt64 DateTime64(9))
        ENGINE MergeTree
        ORDER BY dt)",
        database_name, table_name));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, create_table_query.data(), SQL_NTS));

    auto insert_query = fromUTF8<SQLTCHAR>(std::format(R"(
        INSERT INTO {}.{} VALUES (?, ?))",
        std::string(database_name), table_name));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, insert_query.data(), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
        hstmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 19, 0, &datetime, sizeof(datetime), nullptr));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
        hstmt, 2, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 29, 9, &datetime64, sizeof(datetime64), nullptr));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    auto select_query = fromUTF8<SQLTCHAR>(std::format(R"(
        SELECT * FROM {}.{} WHERE dt = ? AND dt64 = ?)",
        std::string(database_name), table_name));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, select_query.data(), SQL_NTS));
        SQLFreeStmt(hstmt, SQL_CLOSE);
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
        hstmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 19, 0, &datetime, sizeof(datetime), nullptr));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
        hstmt, 2, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 29, 9, &datetime64, sizeof(datetime64), nullptr));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));

    ResultSetReader reader{hstmt};
    reader.fetch();
    EXPECT_TRUE(compareOptionalSqlTimeStamps(reader.getData<SQL_TIMESTAMP_STRUCT>("dt"), datetime));
    EXPECT_TRUE(compareOptionalSqlTimeStamps(reader.getData<SQL_TIMESTAMP_STRUCT>("dt64"), datetime64));
}

