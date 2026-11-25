#include "driver/test/client_test_base.h"

#include <array>
#include <optional>

using NumericBytes = std::array<SQLCHAR, SQL_MAX_NUMERIC_LEN>;

struct DecimalNumericCase {
    std::string format;
    std::string expression;
    SQLSMALLINT precision;
    SQLSCHAR scale;
    SQLCHAR sign;
    std::optional<NumericBytes> bytes;
};

class DecimalGetDataAsNumeric : public ClientTestWithParamBase<DecimalNumericCase> {};

TEST_P(DecimalGetDataAsNumeric, HonorsRequestedPrecisionAndScale) {
    const auto & [format, expression, precision, scale, sign, bytes] = GetParam();
    SCOPED_TRACE(expression);
    auto query = fromUTF8<PTChar>("SELECT " + expression + " FORMAT " + format);
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));

    SQLHDESC desc = SQL_NULL_HDESC;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &desc, 0, nullptr));
    ODBC_CALL_ON_DESC_THROW(desc, SQLSetDescField(desc, 1, SQL_DESC_TYPE, reinterpret_cast<SQLPOINTER>(SQL_C_NUMERIC), 0));
    ODBC_CALL_ON_DESC_THROW(desc, SQLSetDescField(desc, 1, SQL_DESC_PRECISION, reinterpret_cast<SQLPOINTER>(precision), 0));
    ODBC_CALL_ON_DESC_THROW(desc, SQLSetDescField(desc, 1, SQL_DESC_SCALE, reinterpret_cast<SQLPOINTER>(scale), 0));

    SQL_NUMERIC_STRUCT value{};
    SQLLEN length = 0;
    auto result = SQLGetData(hstmt, 1, SQL_C_NUMERIC, &value, sizeof(value), &length);
    if (!bytes) {
        ASSERT_EQ(result, SQL_ERROR);
        EXPECT_NE(extract_diagnostics(hstmt, SQL_HANDLE_STMT).find("[22003]"), std::string::npos);
        return;
    }

    ODBC_CALL_ON_STMT_THROW(hstmt, result);
    EXPECT_EQ(value.precision, precision);
    EXPECT_EQ(value.scale, scale);
    EXPECT_EQ(value.sign, sign);
    EXPECT_EQ(std::to_array(value.val), *bytes);
}

INSTANTIATE_TEST_SUITE_P(
    Decimal, DecimalGetDataAsNumeric,
    ::testing::Values(
        DecimalNumericCase{"ODBCDriver2", "toDecimal128('18446744073709551616', 0)", 20, 0, 1, NumericBytes{0, 0, 0, 0, 0, 0, 0, 0, 1}},
        DecimalNumericCase{"ODBCDriver2", "toDecimal128('18446744073709551616', 0)", 19, 0, 1, std::nullopt},
        DecimalNumericCase{"ODBCDriver2", "toDecimal128('-18446744073709551616.2', 1)", 38, 1, 0, NumericBytes{2, 0, 0, 0, 0, 0, 0, 0, 10}},
        DecimalNumericCase{"ODBCDriver2", "toDecimal128('2000000000000000000000000000000000000', 0)", 38, 1, 1,
            NumericBytes{0x00, 0x00, 0x00, 0x00, 0x40, 0x6d, 0xe8, 0x01, 0xb2, 0x8d, 0xb4, 0xab, 0x21, 0xdc, 0x0b, 0x0f}},
        DecimalNumericCase{"ODBCDriver2", "toDecimal128('20000000000000000000000000000000000000', 0)", 38, 1, 1, std::nullopt},
        DecimalNumericCase{"ODBCDriver2", "toDecimal32('1000', 0)", 3, 0, 1, std::nullopt},
        DecimalNumericCase{"ODBCDriver2", "toDecimal32('1234.56', 2)", 4, 0, 1, NumericBytes{0xd2, 0x04}},
        DecimalNumericCase{"RowBinaryWithNamesAndTypes", "toDecimal32('-660.51', 2)", 5, 2, 0, NumericBytes{3, 2, 1}},
        DecimalNumericCase{"RowBinaryWithNamesAndTypes", "toDecimal64('42.94967297', 8)", 10, 8, 1, NumericBytes{1, 0, 0, 0, 1}}
    )
);
