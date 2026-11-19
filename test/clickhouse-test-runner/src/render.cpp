#include "render.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace runner {

void classifyColumn(ColumnInfo & c) {
    auto integer = [&](SQLSMALLINT signed_type, SQLSMALLINT unsigned_type) {
        c.kind = c.is_unsigned ? ColumnKind::Unsigned : ColumnKind::Signed;
        c.c_type = c.is_unsigned ? unsigned_type : signed_type;
    };
    switch (c.sql_type) {
        case SQL_BIT:
            c.kind = ColumnKind::Bool;
            c.c_type = SQL_C_BIT;
            return;
        case SQL_TINYINT:
            integer(SQL_C_STINYINT, SQL_C_UTINYINT);
            return;
        case SQL_SMALLINT:
            integer(SQL_C_SSHORT, SQL_C_USHORT);
            return;
        case SQL_INTEGER:
            integer(SQL_C_SLONG, SQL_C_ULONG);
            return;
        case SQL_BIGINT:
            integer(SQL_C_SBIGINT, SQL_C_UBIGINT);
            return;
        case SQL_REAL:
            c.kind = ColumnKind::Float32;
            c.c_type = SQL_C_FLOAT;
            return;
        case SQL_FLOAT:
        case SQL_DOUBLE:
            c.kind = ColumnKind::Float64;
            c.c_type = SQL_C_DOUBLE;
            return;
        case SQL_DECIMAL:
        case SQL_NUMERIC:
            // SQL_NUMERIC_STRUCT holds at most 38 decimal digits (a 128-bit value). A wider
            // Decimal (Decimal256) is read as text.
            if (c.precision >= 1 && c.precision <= 38 && c.scale >= 0 && c.scale <= c.precision) {
                c.kind = ColumnKind::Numeric;
                c.c_type = SQL_ARD_TYPE;
            } else {
                c.kind = ColumnKind::Text;
                c.c_type = SQL_C_CHAR;
            }
            return;
        case SQL_TYPE_DATE:
            c.kind = ColumnKind::Date;
            c.c_type = SQL_C_TYPE_DATE;
            return;
        case SQL_TYPE_TIME:
            c.kind = ColumnKind::Time;
            c.c_type = SQL_C_TYPE_TIME;
            return;
        case SQL_TYPE_TIMESTAMP:
            c.kind = ColumnKind::Timestamp;
            c.c_type = SQL_C_TYPE_TIMESTAMP;
            if (c.type_name == "DateTime") {
                c.fraction_digits = 0;
            } else if (c.type_name == "DateTime64" && c.scale >= 0 && c.scale <= 9) {
                c.fraction_digits = static_cast<int>(c.scale);
            } else {
                throw std::runtime_error("clickhouse-test-runner: no TabSeparated renderer for SQL_TYPE_TIMESTAMP column of type "
                    + c.type_name + " with scale " + std::to_string(c.scale));
            }
            return;
        case SQL_GUID:
            c.kind = ColumnKind::Guid;
            c.c_type = SQL_C_GUID;
            return;
        case SQL_UNKNOWN_TYPE: // Nothing (a NULL)
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
            c.kind = ColumnKind::Text;
            c.c_type = SQL_C_CHAR;
            return;
    }
    throw std::runtime_error(
        "clickhouse-test-runner: no TabSeparated renderer for SQL type " + std::to_string(c.sql_type) + " (" + c.type_name + ")");
}

void writeEscaped(std::string & out, std::string_view s) {
    for (const char c : s) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '\'':
                out += "\\'";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\0':
                out += "\\0";
                break;
            default:
                out += c;
        }
    }
}

std::string formatFloat(double value, int bits) {
    if (std::isnan(value))
        return "nan";
    if (std::isinf(value))
        return value > 0 ? "inf" : "-inf";
    if (value == 0)
        return std::signbit(value) ? "-0" : "0";

    // Shortest digits and exponent: d.ddddde±XX.
    char buf[64];
    const auto res = (bits == 32) ? std::to_chars(buf, buf + sizeof(buf), static_cast<float>(value), std::chars_format::scientific)
                                  : std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::scientific);
    std::string s(buf, res.ptr);
    bool neg = false;
    if (s[0] == '-') {
        neg = true;
        s.erase(0, 1);
    }
    const auto e_pos = s.find('e');
    const std::string mantissa = s.substr(0, e_pos);
    const int exp = std::stoi(s.substr(e_pos + 1));
    std::string digits;
    for (const char c : mantissa) {
        if (c != '.')
            digits += c;
    }
    // The value is 0.digits × 10^n.
    const int n = exp + 1;
    const int k = static_cast<int>(digits.size());

    std::string out;
    if (k <= n && n <= 21) {
        out = digits + std::string(n - k, '0');
    } else if (0 < n && n <= 21) {
        out = digits.substr(0, n) + "." + digits.substr(n);
    } else if (-6 < n && n <= 0) {
        out = "0." + std::string(-n, '0') + digits;
    } else {
        out = digits.substr(0, 1);
        if (k > 1)
            out += "." + digits.substr(1);
        out += "e" + std::to_string(n - 1);
    }
    return neg ? "-" + out : out;
}

std::string formatNumeric(const SQL_NUMERIC_STRUCT & value) {
    unsigned __int128 mantissa = 0;
    for (int i = SQL_MAX_NUMERIC_LEN - 1; i >= 0; --i)
        mantissa = (mantissa << 8) | value.val[i];
    std::string digits;
    while (mantissa > 0) {
        digits.insert(digits.begin(), static_cast<char>('0' + static_cast<int>(mantissa % 10)));
        mantissa /= 10;
    }
    if (digits.empty())
        return "0";
    const int scale = value.scale;
    std::string out;
    if (scale <= 0) {
        out = digits + std::string(-scale, '0');
    } else {
        if (static_cast<int>(digits.size()) <= scale)
            digits.insert(0, std::string(scale - digits.size() + 1, '0'));
        std::string integer = digits.substr(0, digits.size() - scale);
        std::string fraction = digits.substr(digits.size() - scale);
        while (!fraction.empty() && fraction.back() == '0')
            fraction.pop_back();
        out = fraction.empty() ? integer : integer + "." + fraction;
    }
    // sign: 1 for a positive value, 0 for a negative one.
    return value.sign == 0 ? "-" + out : out;
}

std::string formatDate(const SQL_DATE_STRUCT & v) {
    char buf[32];
    std::snprintf(
        buf, sizeof(buf), "%04d-%02u-%02u", static_cast<int>(v.year), static_cast<unsigned>(v.month), static_cast<unsigned>(v.day));
    return buf;
}

std::string formatTime(const SQL_TIME_STRUCT & v) {
    char buf[32];
    std::snprintf(buf,
        sizeof(buf),
        "%02u:%02u:%02u",
        static_cast<unsigned>(v.hour),
        static_cast<unsigned>(v.minute),
        static_cast<unsigned>(v.second));
    return buf;
}

std::string formatTimestamp(const SQL_TIMESTAMP_STRUCT & v, int fraction_digits) {
    char buf[64];
    std::snprintf(buf,
        sizeof(buf),
        "%04d-%02u-%02u %02u:%02u:%02u",
        static_cast<int>(v.year),
        static_cast<unsigned>(v.month),
        static_cast<unsigned>(v.day),
        static_cast<unsigned>(v.hour),
        static_cast<unsigned>(v.minute),
        static_cast<unsigned>(v.second));
    std::string out = buf;
    if (fraction_digits > 0) {
        char frac[16];
        std::snprintf(frac, sizeof(frac), "%09u", static_cast<unsigned>(v.fraction));
        out += '.';
        out.append(frac, fraction_digits);
    }
    return out;
}

std::string formatGuid(const SQLGUID & v) {
    char buf[64];
    std::snprintf(buf,
        sizeof(buf),
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        static_cast<unsigned>(v.Data1),
        static_cast<unsigned>(v.Data2),
        static_cast<unsigned>(v.Data3),
        v.Data4[0],
        v.Data4[1],
        v.Data4[2],
        v.Data4[3],
        v.Data4[4],
        v.Data4[5],
        v.Data4[6],
        v.Data4[7]);
    return buf;
}

} // namespace runner
