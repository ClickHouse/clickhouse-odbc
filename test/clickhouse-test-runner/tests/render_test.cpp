#include "render.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>

using namespace runner;

TEST(Render, FormatFloat) {
    // Variables, so that the compiler does not fold 0.1 + 0.2 exactly.
    volatile double a = 0.1;
    volatile double b = 0.2;
    // The expected text is what ClickHouse 26.10 writes for FORMAT TSV.
    struct Case {
        double in;
        int bits;
        const char * want;
    };
    const std::vector<Case> cases = {
        {1e15, 64, "1000000000000000"},
        {1e20, 64, "100000000000000000000"},
        {1e21, 64, "1e21"},
        {1.2345e21, 64, "1.2345e21"},
        {1.2345e20, 64, "123450000000000000000"},
        {0.0001, 64, "0.0001"},
        {1e-6, 64, "0.000001"},
        {1.5e-6, 64, "0.0000015"},
        {1e-7, 64, "1e-7"},
        {1.5e-7, 64, "1.5e-7"},
        {1.5e300, 64, "1.5e300"},
        {a + b, 64, "0.30000000000000004"},
        {5e-324, 64, "5e-324"},
        {-12345678.9, 64, "-12345678.9"},
        {100, 64, "100"},
        {std::nan(""), 64, "nan"},
        {std::numeric_limits<double>::infinity(), 64, "inf"},
        {-std::numeric_limits<double>::infinity(), 64, "-inf"},
        {std::copysign(0.0, -1.0), 64, "-0"},
        {0, 64, "0"},
        {static_cast<double>(0.1f), 32, "0.1"},
        {static_cast<double>(3.4028235e38f), 32, "3.4028235e38"},
        {static_cast<double>(16777216.0f), 32, "16777216"},
    };
    for (const auto & c : cases)
        EXPECT_EQ(c.want, formatFloat(c.in, c.bits)) << c.in << " bits " << c.bits;
}

TEST(Render, WriteEscaped) {
    std::string out;
    writeEscaped(out, std::string("a\tb\\c\nd'e\0\b\f\x01", 13));
    EXPECT_EQ("a\\tb\\\\c\\nd\\'e\\0\\b\\f\x01", out);
    // Composite values reach the application as text. The driver reports them as String, so they
    // are escaped too.
    out.clear();
    writeEscaped(out, "['a','b']");
    EXPECT_EQ("[\\'a\\',\\'b\\']", out);
}

namespace {

SQL_NUMERIC_STRUCT numeric(unsigned __int128 mantissa, int scale, bool negative) {
    SQL_NUMERIC_STRUCT n {};
    n.precision = 38;
    n.scale = static_cast<SQLSCHAR>(scale);
    n.sign = negative ? 0 : 1;
    for (int i = 0; i < SQL_MAX_NUMERIC_LEN; ++i) {
        n.val[i] = static_cast<SQLCHAR>(mantissa & 0xff);
        mantissa >>= 8;
    }
    return n;
}

} // namespace

TEST(Render, FormatNumeric) {
    EXPECT_EQ("1.5", formatNumeric(numeric(150, 2, false)));
    EXPECT_EQ("0", formatNumeric(numeric(0, 2, false)));
    EXPECT_EQ("0", formatNumeric(numeric(0, 2, true)));
    EXPECT_EQ("-0.001", formatNumeric(numeric(100, 5, true)));
    EXPECT_EQ("100", formatNumeric(numeric(100000, 3, false)));
    EXPECT_EQ("12", formatNumeric(numeric(12, 0, false)));
    unsigned __int128 max38 = 1;
    for (int i = 0; i < 38; ++i)
        max38 *= 10;
    EXPECT_EQ("9999999999999999999999999999.9999999999", formatNumeric(numeric(max38 - 1, 10, false)));
}

TEST(Render, DatesAndTimes) {
    EXPECT_EQ("2020-01-02", formatDate(SQL_DATE_STRUCT {2020, 1, 2}));
    EXPECT_EQ("1900-01-01", formatDate(SQL_DATE_STRUCT {1900, 1, 1}));
    EXPECT_EQ("12:34:56", formatTime(SQL_TIME_STRUCT {12, 34, 56}));
    EXPECT_EQ("100:00:00", formatTime(SQL_TIME_STRUCT {100, 0, 0}));
    SQL_TIMESTAMP_STRUCT ts {2020, 1, 1, 10, 0, 0, 120000000};
    EXPECT_EQ("2020-01-01 10:00:00", formatTimestamp(ts, 0));
    EXPECT_EQ("2020-01-01 10:00:00.120", formatTimestamp(ts, 3));
    EXPECT_EQ("2020-01-01 10:00:00.120000000", formatTimestamp(ts, 9));
    ts.fraction = 5;
    EXPECT_EQ("2020-01-01 10:00:00.000000005", formatTimestamp(ts, 9));
    EXPECT_EQ("2020-01-01 10:00:00.0", formatTimestamp(ts, 1));
}

TEST(Render, FormatGuid) {
    SQLGUID g {0x61f0c404, 0x5cb3, 0x11e7, {0x90, 0x7b, 0xa6, 0x00, 0x6a, 0xd3, 0xdb, 0xa0}};
    EXPECT_EQ("61f0c404-5cb3-11e7-907b-a6006ad3dba0", formatGuid(g));
}

TEST(Render, ClassifyColumn) {
    ColumnInfo c;
    c.sql_type = SQL_BIGINT;
    c.is_unsigned = true;
    classifyColumn(c);
    EXPECT_EQ(ColumnKind::Unsigned, c.kind);
    EXPECT_EQ(SQL_C_UBIGINT, c.c_type);

    c = ColumnInfo {};
    c.sql_type = SQL_TINYINT;
    classifyColumn(c);
    EXPECT_EQ(ColumnKind::Signed, c.kind);
    EXPECT_EQ(SQL_C_STINYINT, c.c_type);

    c = ColumnInfo {};
    c.sql_type = SQL_DECIMAL;
    c.precision = 9;
    c.scale = 2;
    classifyColumn(c);
    EXPECT_EQ(ColumnKind::Numeric, c.kind);

    // A SQL_NUMERIC_STRUCT holds 38 digits; Decimal256 is read as text.
    c.precision = 76;
    classifyColumn(c);
    EXPECT_EQ(ColumnKind::Text, c.kind);

    c = ColumnInfo {};
    c.sql_type = SQL_TYPE_TIMESTAMP;
    c.type_name = "DateTime64";
    c.scale = 6;
    classifyColumn(c);
    EXPECT_EQ(ColumnKind::Timestamp, c.kind);
    EXPECT_EQ(6, c.fraction_digits);

    c.type_name = "DateTime";
    classifyColumn(c);
    EXPECT_EQ(0, c.fraction_digits);

    c.type_name = "Unknown";
    EXPECT_THROW(classifyColumn(c), std::runtime_error);

    c = ColumnInfo {};
    c.sql_type = SQL_INTERVAL_DAY;
    EXPECT_THROW(classifyColumn(c), std::runtime_error);
}
