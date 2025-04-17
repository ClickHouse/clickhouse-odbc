#include <gtest/gtest.h>

#include "driver/statement.h"
#include "driver/api/impl/impl.h"

class StatementBindingTest : public testing::Test
{
protected:
    void prepare(const std::string& query) {
        statement.prepareQuery(query);
    }

    void bind(
        SQLUSMALLINT    parameter_number,
        SQLSMALLINT     input_output_type,
        SQLSMALLINT     value_type,
        SQLSMALLINT     parameter_type,
        SQLULEN         column_size,
        SQLSMALLINT     decimal_digits,
        SQLPOINTER      parameter_value_ptr,
        SQLLEN          buffer_length,
        SQLLEN *        StrLen_or_IndPtr
    ) {
        auto res = impl::BindParameter(
            &statement,
            parameter_number,
            input_output_type,
            value_type,
            parameter_type,
            column_size,
            decimal_digits,
            parameter_value_ptr,
            buffer_length,
            StrLen_or_IndPtr
        );
        ASSERT_TRUE(SQL_SUCCEEDED(res));
    }

    Statement::HttpRequestData execute() {
        return statement.prepareHttpRequest();
    }


protected:
    Environment environment{Driver::getInstance()};
    Connection connection{environment};
    Statement statement{connection};
};

TEST_F(StatementBindingTest, FromCCharToString) {
    prepare("select ?, ?, ?");

    char char_value[] = "Test String";
    SQLLEN char_len = SQL_NTS;

    bind(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, 20, 0, char_value, sizeof(char_value), &char_len);
    bind(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, char_value, sizeof(char_value), &char_len);
    bind(3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARCHAR, 20, 0, char_value, sizeof(char_value), &char_len);

    auto [query, params] = execute();
    ASSERT_EQ(query,
        "select "
        "{odbc_positional_1:String}, "
        "{odbc_positional_2:LowCardinality(String)}, "
        "{odbc_positional_3:String}");
    ASSERT_EQ(params.size(), 3);
    ASSERT_EQ(params["param_odbc_positional_1"], char_value);
    ASSERT_EQ(params["param_odbc_positional_2"], char_value);
    ASSERT_EQ(params["param_odbc_positional_3"], char_value);
}

TEST_F(StatementBindingTest, FromCCharToOtherTypes) {
    prepare("select ?, ?, ?, ?, ?, ?");

    char num_as_char[] = "12345";
    char decimal_as_char[] = "123.45";
    char date_as_char[] = "2024-02-13";
    char time_as_char[] = "15:30:45";
    char timestamp_as_char[] = "2024-02-13 15:30:45";
    char guid_as_char[] = "12345678-1234-5678-1234-56789abcdef0";
    SQLLEN char_len = SQL_NTS;

    bind(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_INTEGER, 10, 0, num_as_char, sizeof(num_as_char), &char_len);
    bind(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_DECIMAL, 10, 2, decimal_as_char, sizeof(decimal_as_char), &char_len);
    bind(3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_TYPE_DATE, 10, 0, date_as_char, sizeof(date_as_char), &char_len);
    bind(4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_TYPE_TIME, 8, 0, time_as_char, sizeof(time_as_char), &char_len);
    bind(5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_TYPE_TIMESTAMP, 19, 0, timestamp_as_char, sizeof(timestamp_as_char), &char_len);
    bind(6, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_GUID, 36, 0, guid_as_char, sizeof(guid_as_char), &char_len);

    auto [query, params] = execute();
    ASSERT_EQ(query,
        "select "
        "{odbc_positional_1:Int32}, "
        "{odbc_positional_2:Decimal(10, 2)}, "
        "{odbc_positional_3:Date}, "
        "{odbc_positional_4:LowCardinality(String)}, "
        "{odbc_positional_5:DateTime64(9)}, "
        "{odbc_positional_6:UUID}");
    ASSERT_EQ(params.size(), 6);
    ASSERT_EQ(params["param_odbc_positional_1"], num_as_char);
    ASSERT_EQ(params["param_odbc_positional_2"], decimal_as_char);
    ASSERT_EQ(params["param_odbc_positional_3"], date_as_char);
    ASSERT_EQ(params["param_odbc_positional_4"], time_as_char);
    ASSERT_EQ(params["param_odbc_positional_5"], timestamp_as_char);
    ASSERT_EQ(params["param_odbc_positional_6"], guid_as_char);
}

TEST_F(StatementBindingTest, FromCIntegralTypes) {
    prepare("select ?, ?, ?, ?, ?");

    SQLINTEGER long_value = 12345;
    SQLSMALLINT short_value = 123;
    SQLLEN long_len = sizeof(long_value);
    SQLLEN short_len = sizeof(short_value);

    bind(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &long_value, 0, &long_len);
    bind(2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_SMALLINT, 0, 0, &long_value, 0, &long_len);
    bind(3, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_DECIMAL, 10, 0, &long_value, 0, &long_len);
    bind(4, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_SMALLINT, 0, 0, &short_value, 0, &short_len);
    bind(5, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_INTEGER, 0, 0, &short_value, 0, &short_len);

    auto [query, params] = execute();
    ASSERT_EQ(query,
        "select "
        "{odbc_positional_1:Nullable(Int32)}, "
        "{odbc_positional_2:Nullable(Int16)}, "
        "{odbc_positional_3:Nullable(Decimal(10, 0))}, "
        "{odbc_positional_4:Nullable(Int16)}, "
        "{odbc_positional_5:Nullable(Int32)}");
    ASSERT_EQ(params.size(), 5);
    ASSERT_EQ(params["param_odbc_positional_1"], std::to_string(long_value));
    ASSERT_EQ(params["param_odbc_positional_2"], std::to_string(long_value));
    ASSERT_EQ(params["param_odbc_positional_3"], std::to_string(long_value));
    ASSERT_EQ(params["param_odbc_positional_4"], std::to_string(short_value));
    ASSERT_EQ(params["param_odbc_positional_5"], std::to_string(short_value));
}

TEST_F(StatementBindingTest, FromCFloatTypes) {
    prepare("select ?, ?, ?, ?, ?, ?");

    float float_value = 123.45;
    double double_value = 123.45;
    SQLLEN float_len = sizeof(float_value);
    SQLLEN double_len = sizeof(double_value);

    bind(1, SQL_PARAM_INPUT, SQL_C_FLOAT, SQL_REAL, 0, 0, &float_value, 0, &float_len);
    bind(2, SQL_PARAM_INPUT, SQL_C_FLOAT, SQL_FLOAT, 0, 0, &float_value, 0, &float_len);
    bind(3, SQL_PARAM_INPUT, SQL_C_FLOAT, SQL_DECIMAL, 15, 10, &float_value, 0, &float_len);
    bind(4, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE, 0, 0, &double_value, 0, &double_len);
    bind(5, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_FLOAT, 0, 0, &double_value, 0, &double_len);
    bind(6, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DECIMAL, 15, 10, &double_value, 0, &double_len);

    auto [query, params] = execute();
    ASSERT_EQ(query,
        "select "
        "{odbc_positional_1:Nullable(Float32)}, "
        "{odbc_positional_2:Nullable(Float64)}, "
        "{odbc_positional_3:Nullable(Decimal(15, 10))}, "
        "{odbc_positional_4:Nullable(Float64)}, "
        "{odbc_positional_5:Nullable(Float64)}, "
        "{odbc_positional_6:Nullable(Decimal(15, 10))}");
    ASSERT_EQ(params.size(), 6);
    ASSERT_FLOAT_EQ(std::stof(params["param_odbc_positional_1"]), float_value);
    ASSERT_FLOAT_EQ(std::stof(params["param_odbc_positional_2"]), float_value);
    ASSERT_FLOAT_EQ(std::stof(params["param_odbc_positional_3"]), float_value);
    ASSERT_DOUBLE_EQ(std::stod(params["param_odbc_positional_4"]), double_value);
    ASSERT_DOUBLE_EQ(std::stod(params["param_odbc_positional_5"]), double_value);
    ASSERT_DOUBLE_EQ(std::stod(params["param_odbc_positional_6"]), double_value);
}

TEST_F(StatementBindingTest, FromCDateTime) {
    prepare("select ?, ?, ?");
    SQL_DATE_STRUCT date_value = {2024, 2, 13};
    SQL_TIME_STRUCT time_value = {15, 30, 45};
    SQL_TIMESTAMP_STRUCT timestamp_value = {2024, 2, 13, 15, 30, 45, 123456789};
    SQLLEN date_len = sizeof(date_value);
    SQLLEN time_len = sizeof(time_value);
    SQLLEN timestamp_len = sizeof(timestamp_value);

    bind(1, SQL_PARAM_INPUT, SQL_C_TYPE_DATE, SQL_TYPE_DATE, 0, 0, &date_value, 0, &date_len);
    bind(2, SQL_PARAM_INPUT, SQL_C_TYPE_TIME, SQL_TYPE_TIME, 0, 0, &time_value, 0, &time_len);
    bind(3, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 0, 0, &timestamp_value, 0, &timestamp_len);

    auto [query, params] = execute();
    ASSERT_EQ(query,
        "select "
        "{odbc_positional_1:Nullable(Date)}, "
        "{odbc_positional_2:LowCardinality(String)}, "
        "{odbc_positional_3:Nullable(DateTime64(9))}");
    ASSERT_EQ(params.size(), 3);
    ASSERT_EQ(params["param_odbc_positional_1"], "2024-02-13");
    ASSERT_EQ(params["param_odbc_positional_2"], "15:30:45");
    ASSERT_EQ(params["param_odbc_positional_3"], "2024-02-13 15:30:45.123456789");
}

TEST_F(StatementBindingTest, FromCBit) {
    prepare("select ?, ?, ?");
    bool bit_true = 1;
    bool bit_false = 0;
    SQLLEN bit_len = sizeof(bit_true);

    bind(1, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT, 0, 0, &bit_true, 0, &bit_len);
    bind(2, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT, 0, 0, &bit_false, 0, &bit_len);
    bind(3, SQL_PARAM_INPUT, SQL_C_BIT, SQL_TINYINT, 0, 0, &bit_true, 0, &bit_len);

    auto [query, params] = execute();
    ASSERT_EQ(query,
        "select "
        "{odbc_positional_1:Nullable(UInt8)}, "
        "{odbc_positional_2:Nullable(UInt8)}, "
        "{odbc_positional_3:Nullable(Int8)}");
    ASSERT_EQ(params.size(), 3);
    ASSERT_EQ(params["param_odbc_positional_1"], "1");
    ASSERT_EQ(params["param_odbc_positional_2"], "0");
    ASSERT_EQ(params["param_odbc_positional_3"], "1");
}

TEST_F(StatementBindingTest, FromCBinary) {
    prepare("select ?, ?");
    unsigned char binary_value[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    SQLLEN binary_len = sizeof(binary_value);

    bind(1, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_BINARY, sizeof(binary_value), 0, binary_value, sizeof(binary_value), &binary_len);
    bind(2, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_VARBINARY, sizeof(binary_value), 0, binary_value, sizeof(binary_value), &binary_len);

    auto [query, params] = execute();
    ASSERT_EQ(query,
        "select "
        "{odbc_positional_1:FixedString(5)}, "
        "{odbc_positional_2:LowCardinality(String)}");
    ASSERT_EQ(params.size(), 2);
    ASSERT_EQ(params["param_odbc_positional_1"], std::string(binary_value, binary_value + sizeof(binary_value)));
    ASSERT_EQ(params["param_odbc_positional_2"], std::string(binary_value, binary_value + sizeof(binary_value)));
}


TEST_F(StatementBindingTest, FromCGUID) {
    prepare("select ?, ?");
    SQLGUID guid_value = {0x12345678, 0x1234, 0x5678, {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}};
    SQLLEN guid_len = sizeof(guid_value);

    bind(1, SQL_PARAM_INPUT, SQL_C_GUID, SQL_GUID, 16, 0, &guid_value, 0, &guid_len);
    bind(2, SQL_PARAM_INPUT, SQL_C_GUID, SQL_CHAR, 36, 0, &guid_value, 0, &guid_len);

    auto [query, params] = execute();
    ASSERT_EQ(query,
        "select "
        "{odbc_positional_1:Nullable(UUID)}, "
        "{odbc_positional_2:String}");
    ASSERT_EQ(params.size(), 2);
    ASSERT_EQ(params["param_odbc_positional_1"], "12345678-1234-5678-1234-56789abcdef0");
    ASSERT_EQ(params["param_odbc_positional_2"], "12345678-1234-5678-1234-56789abcdef0");
}

TEST_F(StatementBindingTest, NullValueBinding) {
    prepare("select ?");
    SQLINTEGER null_value;
    SQLLEN null_ind = SQL_NULL_DATA;

    bind(1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &null_value, 0, &null_ind);

    auto [query, params] = execute();
    ASSERT_EQ(query, "select {odbc_positional_1:Nullable(Int32)}");
    ASSERT_EQ(params.size(), 1);
    ASSERT_TRUE(params["param_odbc_positional_1"].empty());
}

TEST_F(StatementBindingTest, FunctionLocate) {
    prepare("select {fn locate(?, ?, ?)}");

    std::string param_1 = "needle";
    bind(1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, param_1.data(), param_1.size(), NULL);

    std::string param_2 = "haystack";
    bind(2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 0, 0, param_2.data(), param_2.size(), NULL);

    int param_3 = 5;
    bind(3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, &param_3, 0, NULL);

    auto [query, params] = execute();
    ASSERT_EQ(query,
        "select locate({odbc_positional_1:LowCardinality(String)},"
        "{odbc_positional_2:LowCardinality(String)},"
        "accurateCast({odbc_positional_3:Nullable(Int32)},'UInt64'))");
    ASSERT_EQ(params.size(), 3);
    ASSERT_EQ(params["param_odbc_positional_1"], "needle");
    ASSERT_EQ(params["param_odbc_positional_2"], "haystack");
    ASSERT_EQ(params["param_odbc_positional_3"], "5");
}
