#pragma once

#include "platform.h"
#include "unicode_t.h"

#include <algorithm>
#include <string>
#include <map>

#include <cstring>

#define lengthof(a) (sizeof(a) / sizeof(a[0]))

struct TypeInfo {
    std::string sql_type_name;
    bool is_unsigned;
    SQLSMALLINT sql_type;
    int32_t column_size;
    int32_t octet_length;

    static constexpr auto string_max_size = 0xFFFFFF;

    inline bool isIntegerType() const {
        return sql_type == SQL_TINYINT || sql_type == SQL_SMALLINT || sql_type == SQL_INTEGER || sql_type == SQL_BIGINT;
    }

    inline bool isStringType() const {
        return sql_type == SQL_VARCHAR;
    }
};

extern const std::map<std::string, TypeInfo> types_g;

inline const TypeInfo & type_info_for(const std::string & type) {
    const auto it = types_g.find(type);
    if (it == types_g.end())
        throw std::runtime_error("unknown type");
    return it->second;
}

// An integer type big enough to hold the integer value that is built from all
// decimal digits of Decimal/Numeric values, as if there is no decimal point.
// Size of this integer defines the upper bound of the "info" the internal
// representation can carry.
// TODO: switch to some 128-bit or even arbitrary-precision unsigned integer type.
using numeric_uint_container_t = std::uint_fast64_t;

SQLSMALLINT convertSQLTypeToCType(SQLSMALLINT sql_type) noexcept;

bool isVerboseType(SQLSMALLINT type) noexcept;
bool isConciseDateTimeIntervalType(SQLSMALLINT sql_type) noexcept;
bool isConciseNonDateTimeIntervalType(SQLSMALLINT sql_type) noexcept;

SQLSMALLINT tryConvertSQLTypeToVerboseType(SQLSMALLINT type) noexcept;
SQLSMALLINT convertSQLTypeToDateTimeIntervalCode(SQLSMALLINT type) noexcept;
SQLSMALLINT convertDateTimeIntervalCodeToSQLType(SQLSMALLINT code, SQLSMALLINT verbose_type) noexcept;

bool isIntervalCode(SQLSMALLINT code) noexcept;
bool intervalCodeHasSecondComponent(SQLSMALLINT code) noexcept;

bool isInputParam(SQLSMALLINT param_io_type) noexcept;
bool isOutputParam(SQLSMALLINT param_io_type) noexcept;
bool isStreamParam(SQLSMALLINT param_io_type) noexcept;

/// Helper structure that represents information about where and
/// how to get or put values when reading or writing bound buffers.
struct BindingInfo {
    SQLSMALLINT c_type = SQL_C_DEFAULT;
    PTR value = nullptr;
    SQLLEN value_max_size = 0;
    SQLLEN * value_size = nullptr;
    SQLLEN * indicator = nullptr;

    // These are relevant only for bound SQL_NUMERIC/SQL_C_NUMERIC or SQL_DECIMAL.
    std::int16_t precision = 0;
    std::int16_t scale = 0;
};

/// Helper structure that represents information about where and
/// how to get or put values when reading or writing bound parameter buffers.
struct ParamBindingInfo
    : public BindingInfo
{
    SQLSMALLINT io_type = SQL_PARAM_INPUT;
    SQLSMALLINT sql_type = SQL_UNKNOWN_TYPE;
    bool nullable = false;
};

/// Helper structure that represents different aspects of parameter info in a prepared query.
struct ParamInfo {
    std::string name;
    std::string tmp_placeholder;
};

struct BoundTypeInfo {
    SQLSMALLINT c_type = SQL_C_DEFAULT;
    SQLSMALLINT sql_type = SQL_UNKNOWN_TYPE;
    SQLLEN value_max_size = 0;
    std::int16_t precision = 0;
    std::int16_t scale = 0;
    bool nullable = false;
};

std::string convertCTypeToDataSourceType(const BoundTypeInfo & type_info);
std::string convertSQLTypeToDataSourceType(const BoundTypeInfo & type_info);
std::string convertSQLOrCTypeToDataSourceType(const BoundTypeInfo & type_info);

namespace value_manip {

    template <typename T>
    inline void reset(T & obj) {
        obj = T{};
    }

    inline void reset(SQL_NUMERIC_STRUCT & numeric) {
        numeric.precision = 0;
        numeric.scale = 0;
        numeric.sign = 0;
        std::fill(std::begin(numeric.val), std::end(numeric.val), 0);
    }

    inline void reset(SQLGUID & guid) {
        guid.Data1 = 0;
        guid.Data2 = 0;
        guid.Data3 = 0;
        std::fill(std::begin(guid.Data4), std::end(guid.Data4), 0);
    }

    inline void reset(SQL_DATE_STRUCT & date) {
        date.year = 0;
        date.month = 0;
        date.day = 0;
    }

    inline void reset(SQL_TIME_STRUCT & time) {
        time.hour = 0;
        time.minute = 0;
        time.second = 0;
    }

    inline void reset(SQL_TIMESTAMP_STRUCT & timestamp) {
        timestamp.year = 0;
        timestamp.month = 0;
        timestamp.day = 0;
        timestamp.hour = 0;
        timestamp.minute = 0;
        timestamp.second = 0;
        timestamp.fraction = 0;
    }

    template <typename T>
    inline std::int16_t getColumnSize(const T & obj, const TypeInfo & type_info) {
        return type_info.column_size;
    }

    inline std::int16_t getColumnSize(const SQL_NUMERIC_STRUCT & numeric, const TypeInfo & type_info) {
        return (numeric.precision == 0 ? type_info.column_size : numeric.precision);
    }

    // TODO: implement getColumnSize() for other types.

    template <typename T>
    inline std::int16_t getDecimalDigits(const T & obj, const TypeInfo & type_info) {
        return 0;
    }

    inline std::int16_t getDecimalDigits(const SQL_NUMERIC_STRUCT & numeric, const TypeInfo & type_info) {
        return numeric.scale;
    }

    // TODO: implement getDecimalDigits() for other types.

    template <typename T>
    struct to {
    };

    template <>
    struct to<std::string> {
        template <typename F>
        static inline std::string from(const F & obj) {
            return std::to_string(obj);
        }

        template <typename F>
        static inline std::string from(const BindingInfo & binding_info) {
            if (!binding_info.value)
                return std::string{};

            const auto * ind_ptr = binding_info.indicator;

            if (ind_ptr) {
                switch (*ind_ptr) {
                    case 0:
                    case SQL_NTS:
                        break;

                    case SQL_NULL_DATA:
                        return std::string{};

                    case SQL_DEFAULT_PARAM: {
                        F obj;
                        reset(obj);
                        return from<F>(obj);
                    }

                    default:
                        if (*ind_ptr == SQL_DATA_AT_EXEC || *ind_ptr < 0)
                            throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
                }
            }

            const auto & obj = *reinterpret_cast<F*>(binding_info.value);
            return from<F>(obj);
        }
    };

    template <>
    inline std::string to<std::string>::from<SQLCHAR *>(const BindingInfo & binding_info) {
        const auto * cstr = reinterpret_cast<const char *>(binding_info.value);

        if (!cstr)
            return std::string{};

        const auto * sz_ptr = binding_info.value_size;
        const auto * ind_ptr = binding_info.indicator;

        if (ind_ptr) {
            switch (*ind_ptr) {
                case 0:
                case SQL_NTS:
                    return std::string{cstr};

                case SQL_NULL_DATA:
                case SQL_DEFAULT_PARAM:
                    return std::string{};

                default:
                    if (*ind_ptr == SQL_DATA_AT_EXEC || *ind_ptr < 0)
                        throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
            }
        }

        if (!sz_ptr || *sz_ptr < 0)
            return std::string{cstr};

        return std::string{cstr, static_cast<std::size_t>(*sz_ptr)};
    }

    template <>
    inline std::string to<std::string>::from<SQLWCHAR *>(SQLWCHAR * const & str) {
        const auto * wcstr = reinterpret_cast<const wchar_t *>(str);
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
        return convert.to_bytes(wcstr);
    }

    template <>
    inline std::string to<std::string>::from<SQLWCHAR *>(const BindingInfo & binding_info) {
        const auto * wcstr = reinterpret_cast<const wchar_t *>(binding_info.value);

        if (!wcstr)
            return std::string{};

        const auto * sz_ptr = binding_info.value_size;
        const auto * ind_ptr = binding_info.indicator;

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;

        if (ind_ptr) {
            switch (*ind_ptr) {
                case 0:
                case SQL_NTS:
                    return convert.to_bytes(wcstr);

                case SQL_NULL_DATA:
                case SQL_DEFAULT_PARAM:
                    return std::string{};

                default:
                    if (*ind_ptr == SQL_DATA_AT_EXEC || *ind_ptr < 0)
                        throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
            }
        }

        if (!sz_ptr || *sz_ptr < 0)
            return convert.to_bytes(wcstr);

        const auto* wcstr_last = wcstr + static_cast<std::size_t>(*sz_ptr) / sizeof(decltype(*wcstr));
        return convert.to_bytes(wcstr, wcstr_last);
    }

    template <>
    inline std::string to<std::string>::from<SQL_NUMERIC_STRUCT>(const SQL_NUMERIC_STRUCT & numeric) {
        numeric_uint_container_t bigint = 0;

        constexpr auto bigint_max = std::numeric_limits<decltype(bigint)>::max();
        constexpr std::uint32_t dec_mult = 10;
        constexpr std::uint32_t byte_mult = 1 << 8;

        for (std::size_t i = 1; i <= lengthof(numeric.val); ++i) {
            const std::uint32_t next_byte_dig = static_cast<unsigned char>(numeric.val[lengthof(numeric.val) - i]);

            if (bigint != 0) {
                if ((bigint_max / byte_mult) < bigint)
                    throw std::runtime_error("Unable to extract data from bound buffer: Numeric value is too big for internal representation");
                bigint *= byte_mult;
            }

            if (next_byte_dig != 0) {
                if ((bigint_max - next_byte_dig) < bigint)
                    throw std::runtime_error("Unable to extract data from bound buffer: Numeric value is too big for internal representation");
                bigint += next_byte_dig;
            }
        }

        const bool bigint_was_zero = (bigint == 0);

        std::string result;
        result.reserve(128);

        for (auto i = numeric.scale; i < 0; ++i) {
            result.push_back('0');
        }

        while (result.size() < numeric.scale || bigint != 0) {
            char next_dig = '0';

            if (bigint != 0) {
                next_dig += bigint % dec_mult;
                bigint /= dec_mult;
            }

            result.push_back(next_dig);

            if (result.size() == numeric.scale)
                result.push_back('.');
        }

        if (result.empty())
            result.push_back('0');
        else if (numeric.sign == 0 && !bigint_was_zero)
            result.push_back('-');

        std::reverse(result.begin(), result.end());
        return result;
    }

    template <>
    inline std::string to<std::string>::from<SQL_NUMERIC_STRUCT>(const BindingInfo & binding_info) {
        if (!binding_info.value)
            return std::string{};

        if (binding_info.precision < 1 || binding_info.precision < binding_info.scale)
            throw std::runtime_error("Bad Numeric specification in binding info");

        const auto * ind_ptr = binding_info.indicator;

        if (ind_ptr) {
            switch (*ind_ptr) {
                case 0:
                case SQL_NTS:
                    break;

                case SQL_NULL_DATA:
                    return std::string{};

                case SQL_DEFAULT_PARAM:
                    return std::string{"0"};

                default:
                    if (*ind_ptr == SQL_DATA_AT_EXEC || *ind_ptr < 0)
                        throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
            }
        }

        auto & numeric = *reinterpret_cast<SQL_NUMERIC_STRUCT *>(binding_info.value);

        // TODO: avoid copy.
        SQL_NUMERIC_STRUCT numeric_copy;
        std::memcpy(&numeric_copy, &numeric, sizeof(numeric_copy));
        numeric_copy.precision = binding_info.precision;
        numeric_copy.scale = binding_info.scale;

        return from<SQL_NUMERIC_STRUCT>(numeric_copy);
    }

    template <>
    inline std::string to<std::string>::from<SQLGUID>(const SQLGUID & guid) {
        char buf[256];
        const auto written = std::snprintf(buf, lengthof(buf), "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            (unsigned int)guid.Data1, (unsigned int)guid.Data2, (unsigned int)guid.Data3,
            (unsigned int)guid.Data4[0], (unsigned int)guid.Data4[1], (unsigned int)guid.Data4[2], (unsigned int)guid.Data4[3],
            (unsigned int)guid.Data4[4], (unsigned int)guid.Data4[5], (unsigned int)guid.Data4[6], (unsigned int)guid.Data4[7]
        );

        if (written < 36 || written >= lengthof(buf))
            buf[0] = '\0';

        return std::string{buf};
    }

    template <>
    inline std::string to<std::string>::from<SQL_DATE_STRUCT>(const SQL_DATE_STRUCT & date) {
        char buf[256];

        const auto written = std::snprintf(buf, lengthof(buf), "%04d-%02d-%02d", (int)date.year, (int)date.month, (int)date.day);
        if (written < 10 || written >= lengthof(buf))
            buf[0] = '\0';

        return std::string{buf};
    }

    template <>
    inline std::string to<std::string>::from<SQL_TIME_STRUCT>(const SQL_TIME_STRUCT & time) {
        char buf[256];

        const auto written = std::snprintf(buf, lengthof(buf), "%02d:%02d:%02d", (int)time.hour, (int)time.minute, (int)time.second);
        if (written < 8 || written >= lengthof(buf))
            buf[0] = '\0';

        return std::string{buf};
    }

    template <>
    inline std::string to<std::string>::from<SQL_TIMESTAMP_STRUCT>(const SQL_TIMESTAMP_STRUCT & timestamp) {
        char buf[256];

        const auto written = std::snprintf(buf, lengthof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                (int)timestamp.year, (int)timestamp.month, (int)timestamp.day,
                (int)timestamp.hour, (int)timestamp.minute, (int)timestamp.second
        );
        if (written < 8 || written >= lengthof(buf)) {
            buf[0] = '\0';
        }
        else if (timestamp.fraction > 0 && timestamp.fraction < 1000000000) {
            const auto written_more = std::snprintf(buf + written, lengthof(buf) - written, ".%09d", (int)timestamp.fraction);
            if (written_more < 2 || written_more >= (lengthof(buf) - written))
                buf[written] = '\0';
        }

        return std::string{buf};
    }

    template <>
    struct to<SQL_NUMERIC_STRUCT> {
        template <typename F>
        static inline SQL_NUMERIC_STRUCT from(const F & obj); // leave unimplemented for general case

        template <typename F>
        static inline SQL_NUMERIC_STRUCT from(const F & obj, const std::int16_t precision, const std::int16_t scale); // leave unimplemented for general case
    };

    template <>
    inline SQL_NUMERIC_STRUCT to<SQL_NUMERIC_STRUCT>::from<std::string>(const std::string & str, const std::int16_t precision, const std::int16_t scale) {
        // If precision == 0 then we try to detect the actual precision and scale from the string and keep them.
        // Othervise, the requested precision and scale will be enforced.

        if (precision < 0 || precision < scale)
            throw std::runtime_error("Bad Numeric specification");

        SQL_NUMERIC_STRUCT res;
        value_manip::reset(res);

        numeric_uint_container_t bigint = 0;

        constexpr auto bigint_max = std::numeric_limits<decltype(bigint)>::max();
        constexpr std::uint32_t dec_mult = 10;
        constexpr std::uint32_t byte_mult = 1 << 8;

        std::size_t left_n = 0;
        std::size_t right_n = 0;
        bool sign_met = false;
        bool dot_met = false;
        bool dig_met = false;

        res.sign = 1;

        for (auto ch : str) {
            switch (ch) {
                case '+':
                case '-': {
                    if (sign_met || dot_met || dig_met)
                        throw std::runtime_error("Cannot interpret '" + str + "' as Numeric");

                    if (ch == '-')
                        res.sign = 0;

                    sign_met = true;
                    break;
                }

                case '.': {
                    if (dot_met)
                        throw std::runtime_error("Cannot interpret '" + str + "' as Numeric");

                    dot_met = true;
                    break;
                }

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                    const std::uint32_t next_dec_dig = static_cast<unsigned char>(ch - '0');

                    if (bigint != 0) {
                        if ((bigint_max / dec_mult) < bigint)
                            throw std::runtime_error("Cannot interpret '" + str + "' as Numeric: value is too big for internal representation");
                        bigint *= dec_mult;
                    }

                    if (next_dec_dig != 0) {
                        if ((bigint_max - next_dec_dig) < bigint)
                            throw std::runtime_error("Cannot interpret '" + str + "' as Numeric: value is too big for internal representation");
                        bigint += next_dec_dig;
                    }

                    if (dot_met)
                        ++right_n;
                    else
                        ++left_n;

                    dig_met = true;
                    break;
                }

                default:
                    throw std::runtime_error("Cannot interpret '" + str + "' as Numeric");
            }
        }

        // Save the actual detected precision and scale for now.
        res.precision = left_n + right_n;
        res.scale = right_n;

        if (bigint == 0)
            res.sign = 1;

        const bool adjust_specs = (precision > 0 && precision != res.precision && scale != res.scale);

        // Adjust the detected scale if requested.
        if (adjust_specs) {
            while (res.scale < scale) {
                if ((bigint_max / dec_mult) < bigint)
                    throw std::runtime_error("Cannot fit source Numeric value '" + str + "' into destination Numeric specification: value is too big for internal representation");
                bigint *= dec_mult;
                ++res.scale;
            }

            while (scale < res.scale) {
                bigint /= dec_mult;
                --res.scale;
            }

            res.precision = precision;
        }

        for (std::size_t i = 0; bigint != 0; ++i) {
            if (i >= lengthof(res.val) || i > res.precision)
                throw std::runtime_error("Cannot fit source Numeric value '" + str + "' into destination Numeric specification: value is too big for ODBC Numeric representation");

            res.val[i] = static_cast<std::uint32_t>(bigint % byte_mult);
            bigint /= byte_mult;
        }

        return res;
    }

    template <>
    inline SQL_NUMERIC_STRUCT to<SQL_NUMERIC_STRUCT>::from<std::string>(const std::string & str) {
        return from<std::string>(str, 0, 0); // autodetect precision and scale from string representation
    }

    template <>
    struct to<SQLGUID> {
        template <typename F>
        static inline SQLGUID from(const F & str); // leave unimplemented for general case
    };

    template <>
    inline SQLGUID to<SQLGUID>::from<std::string>(const std::string & str) {
        unsigned int Data1 = 0;
        unsigned int Data2 = 0;
        unsigned int Data3 = 0;
        unsigned int Data4[8] = { 0 };
        char guard = '\0';

        const auto read = std::sscanf(str.c_str(), "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x%c",
            &Data1, &Data2, &Data3,
            &Data4[0], &Data4[1], &Data4[2], &Data4[3],
            &Data4[4], &Data4[5], &Data4[6], &Data4[7],
            &guard
        );

        if (read != 11) // All 'DataN' must be successfully read, but not the 'guard'.
            throw std::runtime_error("Cannot interpret '" + str + "' as GUID");

        SQLGUID res;

        res.Data1 = Data1;
        res.Data2 = Data2;
        res.Data3 = Data3;
        std::copy(std::begin(Data4), std::end(Data4), std::begin(res.Data4));

        return res;
    }

} // namespace convert

template <typename T> constexpr inline SQLSMALLINT getCTypeFor(); // leave unimplemented for general case
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLCHAR *            >() { return SQL_C_CHAR;           }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLWCHAR *           >() { return SQL_C_WCHAR;          }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLSCHAR             >() { return SQL_C_STINYINT;       }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLCHAR              >() { return SQL_C_UTINYINT;       }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLSMALLINT          >() { return SQL_C_SSHORT;         }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLUSMALLINT         >() { return SQL_C_USHORT;         }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLINTEGER           >() { return SQL_C_SLONG;          }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLUINTEGER          >() { return SQL_C_ULONG;          }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLBIGINT            >() { return SQL_C_SBIGINT;        }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLUBIGINT           >() { return SQL_C_UBIGINT;        }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLREAL              >() { return SQL_C_FLOAT;          }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLDOUBLE            >() { return SQL_C_DOUBLE;         }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQLGUID              >() { return SQL_C_GUID;           }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQL_NUMERIC_STRUCT   >() { return SQL_C_NUMERIC;        }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQL_DATE_STRUCT      >() { return SQL_C_TYPE_DATE;      }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQL_TIME_STRUCT      >() { return SQL_C_TYPE_TIME;      }
template <> constexpr inline SQLSMALLINT getCTypeFor< SQL_TIMESTAMP_STRUCT >() { return SQL_C_TYPE_TIMESTAMP; }

template <typename T>
inline T readReadyDataTo(const BindingInfo & binding_info) {
    switch (binding_info.c_type) {
        case SQL_C_CHAR:           return value_manip::to<T>::template from< SQLCHAR *            >(binding_info);
        case SQL_C_WCHAR:          return value_manip::to<T>::template from< SQLWCHAR *           >(binding_info);
        case SQL_C_BIT:            return value_manip::to<T>::template from< SQLCHAR              >(binding_info);
        case SQL_C_TINYINT:        return value_manip::to<T>::template from< SQLSCHAR             >(binding_info);
        case SQL_C_STINYINT:       return value_manip::to<T>::template from< SQLSCHAR             >(binding_info);
        case SQL_C_UTINYINT:       return value_manip::to<T>::template from< SQLCHAR              >(binding_info);
        case SQL_C_SHORT:          return value_manip::to<T>::template from< SQLSMALLINT          >(binding_info);
        case SQL_C_SSHORT:         return value_manip::to<T>::template from< SQLSMALLINT          >(binding_info);
        case SQL_C_USHORT:         return value_manip::to<T>::template from< SQLUSMALLINT         >(binding_info);
        case SQL_C_LONG:           return value_manip::to<T>::template from< SQLINTEGER           >(binding_info);
        case SQL_C_SLONG:          return value_manip::to<T>::template from< SQLINTEGER           >(binding_info);
        case SQL_C_ULONG:          return value_manip::to<T>::template from< SQLUINTEGER          >(binding_info);
        case SQL_C_SBIGINT:        return value_manip::to<T>::template from< SQLBIGINT            >(binding_info);
        case SQL_C_UBIGINT:        return value_manip::to<T>::template from< SQLUBIGINT           >(binding_info);
        case SQL_C_FLOAT:          return value_manip::to<T>::template from< SQLREAL              >(binding_info);
        case SQL_C_DOUBLE:         return value_manip::to<T>::template from< SQLDOUBLE            >(binding_info);
        case SQL_C_BINARY:         return value_manip::to<T>::template from< SQLCHAR *            >(binding_info);
        case SQL_C_GUID:           return value_manip::to<T>::template from< SQLGUID              >(binding_info);
//      case SQL_C_BOOKMARK:       return value_manip::to<T>::template from< BOOKMARK             >(binding_info);
//      case SQL_C_VARBOOKMARK:    return value_manip::to<T>::template from< SQLCHAR *            >(binding_info);

        case SQL_C_NUMERIC:        return value_manip::to<T>::template from< SQL_NUMERIC_STRUCT   >(binding_info);

        case SQL_C_DATE:
        case SQL_C_TYPE_DATE:      return value_manip::to<T>::template from< SQL_DATE_STRUCT      >(binding_info);

        case SQL_C_TIME:
        case SQL_C_TYPE_TIME:      return value_manip::to<T>::template from< SQL_TIME_STRUCT      >(binding_info);

        case SQL_C_TIMESTAMP:
        case SQL_C_TYPE_TIMESTAMP: return value_manip::to<T>::template from< SQL_TIMESTAMP_STRUCT >(binding_info);

        default:
            throw std::runtime_error("Unable to extract data from bound buffer: source type representation not supported");
    }
}
