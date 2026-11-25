#include "driver/test/client_test_base.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <tuple>

using DecimalNumericCase = std::tuple<std::string, std::string, SQLSCHAR, SQLCHAR, std::array<SQLCHAR, SQL_MAX_NUMERIC_LEN>>;

class DecimalGetDataAsNumeric : public ClientTestWithParamBase<DecimalNumericCase> {};

TEST_P(DecimalGetDataAsNumeric, PreservesValue) {
    const auto & [format, expr, scale, sign, bytes] = GetParam();
    auto query = fromUTF8<PTChar>("SELECT " + expr + " FORMAT " + format);
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(query.data()), SQL_NTS));
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));

    SQLHDESC desc = SQL_NULL_HDESC;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, &desc, 0, nullptr));
    ODBC_CALL_ON_DESC_THROW(desc, SQLSetDescField(desc, 1, SQL_DESC_TYPE, reinterpret_cast<SQLPOINTER>(SQL_C_NUMERIC), 0));
    ODBC_CALL_ON_DESC_THROW(desc, SQLSetDescField(desc, 1, SQL_DESC_PRECISION, reinterpret_cast<SQLPOINTER>(38), 0));
    ODBC_CALL_ON_DESC_THROW(desc, SQLSetDescField(desc, 1, SQL_DESC_SCALE, reinterpret_cast<SQLPOINTER>(scale), 0));

    SQL_NUMERIC_STRUCT value{};
    SQLLEN length = 0;
    ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(hstmt, 1, SQL_C_NUMERIC, &value, sizeof(value), &length));
    EXPECT_EQ(value.precision, 38);
    EXPECT_EQ(value.scale, scale);
    EXPECT_EQ(value.sign, sign);
    EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), std::begin(value.val)));
}

INSTANTIATE_TEST_SUITE_P(
    Decimal, DecimalGetDataAsNumeric,
    ::testing::Values(
        DecimalNumericCase{"ODBCDriver2", "toDecimal128('18446744073709551616', 0)", 0, 1, {0, 0, 0, 0, 0, 0, 0, 0, 1}},
        DecimalNumericCase{"ODBCDriver2", "toDecimal128('-18446744073709551616.2', 1)", 1, 0, {2, 0, 0, 0, 0, 0, 0, 0, 10}},
        DecimalNumericCase{"RowBinaryWithNamesAndTypes", "toDecimal32('-660.51', 2)", 2, 0, {3, 2, 1}},
        DecimalNumericCase{"RowBinaryWithNamesAndTypes", "toDecimal64('42.94967297', 8)", 8, 1, {1, 0, 0, 0, 1}}
    )
);
