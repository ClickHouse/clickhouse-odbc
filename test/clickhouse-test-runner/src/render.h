#pragma once

// Writes the values that the driver returned through SQLGetData as the text that the server
// writes for the `TabSeparated` format. The upstream `.reference` files hold the server's text, so
// a test passes only when the driver read and converted every value correctly.
//
// The renderer is type-directed: the C type that the shim reads a column with, and the text it
// writes, follow the SQL type and the type name that the driver reports for the column
// (SQLDescribeCol, SQLColAttribute). The driver reports a base type name only, without
// Nullable(...), LowCardinality(...) and the parameters: `String` for Array, Tuple, Map, Enum,
// IPv4, IPv6, Int128, Time64 and other types that it gives to the application as text. The shim
// writes those by the rules of a String.

#include <sql.h>
#include <sqlext.h>

#include <string>
#include <string_view>

namespace runner {

// How the shim reads and writes the values of one result column.
enum class ColumnKind {
    Bool,      // SQL_BIT, as SQL_C_BIT
    Signed,    // SQL_TINYINT .. SQL_BIGINT, signed C type of the same size
    Unsigned,  // the same, SQL_DESC_UNSIGNED = SQL_TRUE
    Float32,   // SQL_REAL, as SQL_C_FLOAT
    Float64,   // SQL_FLOAT, SQL_DOUBLE, as SQL_C_DOUBLE
    Numeric,   // SQL_DECIMAL, SQL_NUMERIC with precision <= 38, as SQL_C_NUMERIC (SQL_ARD_TYPE)
    Date,      // SQL_TYPE_DATE, as SQL_C_TYPE_DATE
    Time,      // SQL_TYPE_TIME, as SQL_C_TYPE_TIME
    Timestamp, // SQL_TYPE_TIMESTAMP, as SQL_C_TYPE_TIMESTAMP
    Guid,      // SQL_GUID, as SQL_C_GUID
    Text,      // everything else, as SQL_C_CHAR
};

struct ColumnInfo {
    std::string name;
    SQLSMALLINT sql_type = SQL_UNKNOWN_TYPE;
    SQLULEN column_size = 0;
    SQLSMALLINT decimal_digits = 0;
    SQLSMALLINT nullable = SQL_NULLABLE_UNKNOWN;
    std::string type_name; // SQL_DESC_TYPE_NAME
    bool is_unsigned = false;
    SQLLEN precision = 0; // SQL_DESC_PRECISION
    SQLLEN scale = 0;     // SQL_DESC_SCALE

    ColumnKind kind = ColumnKind::Text;
    SQLSMALLINT c_type = SQL_C_CHAR;
    // The number of fraction digits of a timestamp (DateTime64(P)).
    int fraction_digits = 0;
};

// Selects the kind and the C type of a column from its SQL type and type name. Throws
// std::runtime_error for a combination that the renderer does not know, so that a test cannot
// pass by accident.
void classifyColumn(ColumnInfo & column);

// Top-level TabSeparated escaping (writeEscapedString): `\\`, `\'`, `\b`, `\f`, `\n`, `\r`,
// `\t`, `\0`.
void writeEscaped(std::string & out, std::string_view s);

// The shortest text that reads back as the same value, as ClickHouse writes it: positional
// notation when the decimal exponent is in [-6, 21), else scientific without a `+` sign or
// exponent padding (`1e21`, `1.5e-7`). `nan`, `inf`, `-inf`, `-0`. bits is 32 or 64.
std::string formatFloat(double value, int bits);

// A decimal without trailing zeros in the fraction (`1.5`, not `1.50`).
std::string formatNumeric(const SQL_NUMERIC_STRUCT & value);

std::string formatDate(const SQL_DATE_STRUCT & value);
std::string formatTime(const SQL_TIME_STRUCT & value);
// fraction_digits: 0 for DateTime, P for DateTime64(P). The fraction is in nanoseconds.
std::string formatTimestamp(const SQL_TIMESTAMP_STRUCT & value, int fraction_digits);
std::string formatGuid(const SQLGUID & value);

} // namespace runner
