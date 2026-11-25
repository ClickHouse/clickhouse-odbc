#include "driver/platform/platform.h"
#include "driver/test/client_test_base.h"
#include "driver/test/client_utils.h"

#include <gmock/gmock.h>

#include <array>
#include <bit>
#include <cstdint>
#include <string>

namespace {

struct NumericResult {
    const char * name;
    std::string expression;
    const char * wire_text;
    SQLCHAR precision;
    SQLSCHAR scale;
    SQLCHAR sign;
    std::array<SQLCHAR, SQL_MAX_NUMERIC_LEN> magnitude;
    bool fits_precision = true;
    bool use_descriptor = true;
    const char * format = "ODBCDriver2";
};

void PrintTo(const NumericResult & result, std::ostream * out) {
    *out << result.name;
}

// Send the same floating-point values even on servers with different decimal literal parsers.
std::string float64(double value) {
    return "reinterpretAsFloat64(toUInt64(" + std::to_string(std::bit_cast<std::uint64_t>(value)) + "))";
}

std::string float32(float value) {
    return "reinterpretAsFloat32(toUInt32(" + std::to_string(std::bit_cast<std::uint32_t>(value)) + "))";
}

class ScientificNumeric : public ClientTestWithParamBase<NumericResult> {
protected:
    void setNumericDescriptor(SQLCHAR precision, SQLSCHAR scale) {
        SQLHDESC ard = nullptr;
        STMT_OK(SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr));
        ODBC_CALL_ON_DESC_THROW(ard, SQLSetDescField(ard, 2, SQL_DESC_TYPE,
            reinterpret_cast<SQLPOINTER>(SQL_C_NUMERIC), 0));
        ODBC_CALL_ON_DESC_THROW(ard, SQLSetDescField(ard, 2, SQL_DESC_PRECISION,
            reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(precision)), 0));
        ODBC_CALL_ON_DESC_THROW(ard, SQLSetDescField(ard, 2, SQL_DESC_SCALE,
            reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(scale)), 0));
    }

    void fetchRow() {
        const auto & input = GetParam();
        auto query = fromUTF8<PTChar>("SELECT " + input.expression + " AS raw, raw AS number FORMAT " + input.format);
        STMT_OK(SQLExecDirect(hstmt, ptcharCast(query.data()), SQL_NTS));
        STMT_OK(SQLFetch(hstmt));
        SQLCHAR text[64]{};
        SQLLEN text_length = 0;
        STMT_OK(SQLGetData(hstmt, 1, SQL_C_CHAR, text, sizeof(text), &text_length));
        ASSERT_GE(text_length, 0);
        EXPECT_EQ(toUTF8(text, text_length), input.wire_text);
    }
};

TEST_P(ScientificNumeric, FetchUsesRequestedDescriptor) {
    const auto & input = GetParam();
    ASSERT_NO_FATAL_FAILURE(fetchRow());
    if (input.use_descriptor)
        setNumericDescriptor(input.precision, input.scale);

    SQL_NUMERIC_STRUCT value{};
    SQLLEN indicator = 0;
    const SQLSMALLINT target_type = input.use_descriptor ? SQL_ARD_TYPE : SQL_C_NUMERIC;
    const auto result = SQLGetData(hstmt, 2, target_type, &value, sizeof(value), &indicator);
    if (!input.fits_precision) {
        EXPECT_EQ(result, SQL_ERROR);
        EXPECT_THAT(extract_diagnostics(hstmt, SQL_HANDLE_STMT), ::testing::HasSubstr("[22003]"));
        return;
    }

    STMT_OK(result);
    EXPECT_EQ(indicator, sizeof(value));
    EXPECT_EQ(value.precision, input.precision);
    EXPECT_EQ(value.scale, input.scale);
    EXPECT_EQ(value.sign, input.sign);
    EXPECT_THAT(value.val, ::testing::ElementsAreArray(input.magnitude));
}

INSTANTIATE_TEST_SUITE_P(
    Numeric,
    ScientificNumeric,
    ::testing::Values(
        NumericResult{"SmallDouble", float64(1e-7), "1e-7", 10, 9, 1, {0x64}},
        NumericResult{"NegativeDouble", float64(-2.5e-8), "-2.5e-8", 14, 10, 0, {0xfa}},
        NumericResult{"SmallFloat", float32(1e-7f), "1e-7", 9, 8, 1, {0x0a}},
        NumericResult{"NullableDouble", "toNullable(" + float64(1.5e-7) + ")", "1.5e-7", 18, 8, 1, {0x0f}},
        NumericResult{"DefaultScale", float64(1e-7), "1e-7", 38, 0, 1, {}, true, false},
        NumericResult{"LargeDouble", float64(1e21), "1e21", 19, -3, 1,
            {0x00, 0x00, 0x64, 0xa7, 0xb3, 0xb6, 0xe0, 0x0d}},
        NumericResult{"PrecisionLimit", float64(1e21), "1e21", 18, -3, 1, {}, false},
        NumericResult{"StringAlias", "toString(" + float64(1e-7) + ")", "1e-7", 10, 9, 1, {0x64}},
        NumericResult{"RowBinaryStringAlias", "toString(" + float64(1e-7) + ")", "1e-7", 10, 9, 1, {0x64},
            true, true, "RowBinaryWithNamesAndTypes"},
        NumericResult{"UppercaseExponent", "'1.25E+2'", "1.25E+2", 6, 1, 1, {0xe2, 0x04}},
        NumericResult{"DecimalControl", "CAST('12.34' AS Decimal(9, 2))", "12.34", 9, 3, 1, {0x34, 0x30}}
    ),
    [](const auto & info) { return info.param.name; }
);

}
