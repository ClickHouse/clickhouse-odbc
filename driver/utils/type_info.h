#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/utils.h"
#include "driver/utils/conversion.h"
#include "driver/exception.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <limits>
#include <map>

#include <cstring>

#define lengthof(a) (sizeof(a) / sizeof(a[0]))

struct TypeInfo {
    std::string sql_type_name;
    bool is_unsigned;
    SQLSMALLINT sql_type;

    // https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/column-size-decimal-digits-transfer-octet-length-and-display-size
    int32_t column_size;  // max width of value in textual represntation, e.g. number of decimal digits fror numeric types.
    int32_t octet_length; // max binary size of value in memory.

    static constexpr auto string_max_size = 0xFFFFFF;

    inline bool isIntegerType() const noexcept {
        return sql_type == SQL_TINYINT || sql_type == SQL_SMALLINT || sql_type == SQL_INTEGER || sql_type == SQL_BIGINT;
    }

    inline bool isBufferType() const noexcept {
        return
            sql_type == SQL_CHAR || sql_type == SQL_VARCHAR || sql_type == SQL_LONGVARCHAR ||
            sql_type == SQL_WCHAR || sql_type == SQL_WVARCHAR || sql_type == SQL_WLONGVARCHAR ||
            sql_type == SQL_BINARY || sql_type == SQL_VARBINARY || sql_type == SQL_LONGVARBINARY
        ;
    }

    inline bool isWideCharStringType() const noexcept {
        return sql_type == SQL_WCHAR || sql_type == SQL_WVARCHAR || sql_type == SQL_WLONGVARCHAR;
    }
};

extern const std::map<std::string, TypeInfo> types_g;

inline const TypeInfo & type_info_for(const std::string & type) {
    const auto it = types_g.find(type);
    if (it == types_g.end())
        throw std::runtime_error("unknown type");
    return it->second;
}

enum class DataSourceTypeId {
    Unknown,
    Date,
    DateTime,
    Decimal,
    Decimal32,
    Decimal64,
    Decimal128,
    FixedString,
    Float32,
    Float64,
    Int8,
    Int16,
    Int32,
    Int64,
    Nothing,
    String,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    UUID
};

DataSourceTypeId convertUnparametrizedTypeNameToTypeId(const std::string & type_name);
std::string convertTypeIdToUnparametrizedCanonicalTypeName(DataSourceTypeId type_id);

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
    SQLPOINTER value = nullptr;
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
    bool is_nullable = false;
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
    bool is_nullable = false;
};

std::string convertCTypeToDataSourceType(const BoundTypeInfo & type_info);
std::string convertSQLTypeToDataSourceType(const BoundTypeInfo & type_info);
std::string convertSQLOrCTypeToDataSourceType(const BoundTypeInfo & type_info);
bool isMappedToStringDataSourceType(SQLSMALLINT sql_type, SQLSMALLINT c_type) noexcept;

// Directly write raw bytes to the buffer, respecting its size.
// All lengths are in bytes. If 'out_value_max_length == 0',
// assume 'out_value' is able to hold the entire 'in_value'.
// Throw exceptions on some detected errors, but tolerate right truncations.
template <typename LengthType1, typename LengthType2>
inline void fillOutputBufferInternal(
    const void * in_value,
    LengthType1 in_value_length,
    void * out_value,
    LengthType2 out_value_max_length
) {
    if (in_value_length < 0 || (in_value_length > 0 && !in_value))
        throw SqlException("Invalid string or buffer length", "HY090");

    if (in_value_length > 0 && out_value) {
        if (out_value_max_length < 0)
            throw SqlException("Invalid string or buffer length", "HY090");

        auto bytes_to_copy = in_value_length;

        if (out_value_max_length > 0 && out_value_max_length < bytes_to_copy)
            bytes_to_copy = out_value_max_length;

        std::memcpy(out_value, in_value, bytes_to_copy);
    }
}

// Directly write raw bytes to the buffer.
// Throw on all errors, including right truncations.
template <typename LengthType1, typename LengthType2, typename LengthType3>
inline SQLRETURN fillOutputBuffer(
    const void * in_value,
    LengthType1 in_value_length,
    void * out_value,
    LengthType2 out_value_max_length,
    LengthType3 * out_value_length
) {
    fillOutputBufferInternal(
        in_value,
        in_value_length,
        out_value,
        out_value_max_length
    );

    if (out_value_length)
        *out_value_length = in_value_length;

    if (in_value_length > out_value_max_length)
        throw SqlException("String data, right truncated", "01004", SQL_SUCCESS_WITH_INFO);

    return SQL_SUCCESS;
}

// Change encoding, when appropriate, and write the result to the buffer.
// Extra string copy happens here for wide char strings, and strings that require encoding change.
template <typename CharType, typename LengthType1, typename LengthType2, typename ConversionContext>
inline SQLRETURN fillOutputString(
    const std::string & in_value,
    void * out_value,
    LengthType1 out_value_max_length,
    LengthType2 * out_value_length,
    bool in_length_in_bytes,
    bool out_length_in_bytes,
    bool ensure_nts,
    ConversionContext && context
) {
    if (out_value) {
        if (out_value_max_length <= 0)
            throw SqlException("Invalid string or buffer length", "HY090");

        if (out_length_in_bytes && (out_value_max_length % sizeof(CharType)) != 0)
            throw SqlException("Invalid string or buffer length", "HY090");
    }

    auto converted = fromUTF8<CharType>(in_value, context);

    const auto converted_length_in_symbols = converted.size();
    const auto converted_length_in_bytes = converted_length_in_symbols * sizeof(CharType);
    const auto out_value_max_length_in_symbols = (out_length_in_bytes ? (out_value_max_length / sizeof(CharType)) : out_value_max_length);
    const auto out_value_max_length_in_bytes = (out_length_in_bytes ? out_value_max_length : (out_value_max_length * sizeof(CharType)));

    fillOutputBufferInternal(
        converted.data(),
        converted_length_in_bytes,
        out_value,
        out_value_max_length_in_bytes
    );

    context.string_pool.retireString(std::move(converted));

    if (out_value_length) {
        if (out_length_in_bytes)
            *out_value_length = converted_length_in_bytes;
        else
            *out_value_length = converted_length_in_symbols;
    }

    if (ensure_nts && out_value) {
        if (converted_length_in_symbols < out_value_max_length_in_symbols)
            reinterpret_cast<CharType *>(out_value)[converted_length_in_symbols] = CharType{};
        else if (out_value_max_length_in_symbols > 0)
            reinterpret_cast<CharType *>(out_value)[out_value_max_length_in_symbols - 1] = CharType{};
        else
            throw SqlException("Invalid string or buffer length", "HY090");
    }

    if ((converted_length_in_symbols + 1) > out_value_max_length_in_symbols) // +1 for null terminating character
        throw SqlException("String data, right truncated", "01004", SQL_SUCCESS_WITH_INFO);

    return SQL_SUCCESS;
}

template <typename CharType, typename LengthType1, typename LengthType2, typename ConversionContext = DefaultConversionContext>
inline SQLRETURN fillOutputString(
    const std::string & in_value,
    void * out_value,
    LengthType1 out_value_max_length,
    LengthType2 * out_value_length,
    bool length_in_bytes,
    ConversionContext && context = ConversionContext{}
) {
    return fillOutputString<CharType>(
        in_value,
        out_value,
        out_value_max_length,
        out_value_length,
        length_in_bytes,
        length_in_bytes,
        true,
        std::forward<ConversionContext>(context)
    );
}

// If ObjectType is a pointer type then obj is treated as an integer corrsponding to the value of that pointer itself.
template <typename ObjectType, typename LengthType1, typename LengthType2>
inline SQLRETURN fillOutputPOD(
    const ObjectType & obj,
    void * out_value,
    LengthType1 out_value_max_length,
    LengthType2 * out_value_length
) {
    return fillOutputBuffer(
        &obj,
        sizeof(obj),
        out_value,
        out_value_max_length,
        out_value_length
    );
}

template <typename ObjectType, typename LengthType1>
inline SQLRETURN fillOutputPOD(
    const ObjectType & obj,
    void * out_value,
    LengthType1 * out_value_length
) {
    return fillOutputPOD(
        obj,
        out_value,
        sizeof(obj),
        out_value_length
    );
}

template <typename LengthType1, typename LengthType2>
inline SQLRETURN fillOutputNULL(
    void * out_value,
    LengthType1 out_value_max_length,
    LengthType2 * out_value_length
) {
    if (!out_value_length)
        throw SqlException("Indicator variable required but not supplied", "22002");

    *out_value_length = SQL_NULL_DATA;

    return SQL_SUCCESS;
}

namespace value_manip {

    template <typename T>
    inline void to_null(T & obj) {
        obj = T{};
    }

    inline void to_null(std::string & str) {
        str.clear();
    }

    inline void to_null(SQL_NUMERIC_STRUCT & numeric) {
        numeric.precision = 0;
        numeric.scale = 0;
        numeric.sign = 0;
        std::fill(std::begin(numeric.val), std::end(numeric.val), 0);
    }

    inline void to_null(SQLGUID & guid) {
        guid.Data1 = 0;
        guid.Data2 = 0;
        guid.Data3 = 0;
        std::fill(std::begin(guid.Data4), std::end(guid.Data4), 0);
    }

    inline void to_null(SQL_DATE_STRUCT & date) {
        date.year = 0;
        date.month = 0;
        date.day = 0;
    }

    inline void to_null(SQL_TIME_STRUCT & time) {
        time.hour = 0;
        time.minute = 0;
        time.second = 0;
    }

    inline void to_null(SQL_TIMESTAMP_STRUCT & timestamp) {
        timestamp.year = 0;
        timestamp.month = 0;
        timestamp.day = 0;
        timestamp.hour = 0;
        timestamp.minute = 0;
        timestamp.second = 0;
        timestamp.fraction = 0;
    }

    template <typename T>
    inline void to_default(T & obj) {
        return to_null(obj);
    }

    template <typename T>
    static void normalize_date(T & date) {
        if (date.year == 0)
            date.year = 1970;

        if (date.month == 0)
            date.month = 1;

        if (date.day == 0)
            date.day = 1;
    }

} // namespace value_manip

template <typename T>
struct SimpleTypeWrapper {
    explicit SimpleTypeWrapper() {
        value_manip::to_null(value);
    }

    template <typename U>
    explicit SimpleTypeWrapper(U && val)
        : value(std::forward<U>(val))
    {
    }

    T value;
};

// Values stored exactly as they are written on wire in ODBCDriver2 format.
struct WireTypeAnyAsString
    : public SimpleTypeWrapper<std::string>
{
    using SimpleTypeWrapper<std::string>::SimpleTypeWrapper;
};

// Date stored exactly as it is represented on wire in RowBinaryWithNamesAndTypes format.
struct WireTypeDateAsInt
    : public SimpleTypeWrapper<std::uint16_t>
{
    using SimpleTypeWrapper<std::uint16_t>::SimpleTypeWrapper;
};

// DateTime stored exactly as it is represented on wire in RowBinaryWithNamesAndTypes format.
struct WireTypeDateTimeAsInt
    : public SimpleTypeWrapper<std::uint32_t>
{
    using SimpleTypeWrapper<std::uint32_t>::SimpleTypeWrapper;
};

template <DataSourceTypeId Id> struct DataSourceType; // Leave unimplemented for general case.

template <>
struct DataSourceType<DataSourceTypeId::Date>
    : public SimpleTypeWrapper<SQL_DATE_STRUCT>
{
    using SimpleTypeWrapper<SQL_DATE_STRUCT>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::DateTime>
    : public SimpleTypeWrapper<SQL_TIMESTAMP_STRUCT>
{
    using SimpleTypeWrapper<SQL_TIMESTAMP_STRUCT>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::Decimal> {
    // An integer type big enough to hold the integer value that is built from all
    // decimal digits of Decimal/Numeric values, as if there is no decimal point.
    // Size of this integer defines the upper bound of the "info" the internal
    // representation can carry.
    // TODO: switch to some 128-bit or even arbitrary-precision unsigned integer type.
    using ContainerIntType = std::uint_fast64_t;

    ContainerIntType value = 0;
    std::int8_t sign = 0;
    std::int16_t precision = 0;
    std::int16_t scale = 0;
};

template <>
struct DataSourceType<DataSourceTypeId::Decimal32>
    : public DataSourceType<DataSourceTypeId::Decimal>
{
};

template <>
struct DataSourceType<DataSourceTypeId::Decimal64>
    : public DataSourceType<DataSourceTypeId::Decimal>
{
};

template <>
struct DataSourceType<DataSourceTypeId::Decimal128>
    : public DataSourceType<DataSourceTypeId::Decimal>
{
};

template <>
struct DataSourceType<DataSourceTypeId::FixedString>
    : public SimpleTypeWrapper<std::string>
{
    using SimpleTypeWrapper<std::string>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::Float32>
    : public SimpleTypeWrapper<float>
{
    using SimpleTypeWrapper<float>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::Float64>
    : public SimpleTypeWrapper<double>
{
    using SimpleTypeWrapper<double>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::Int8>
    : public SimpleTypeWrapper<std::int8_t>
{
    using SimpleTypeWrapper<std::int8_t>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::Int16>
    : public SimpleTypeWrapper<std::int16_t>
{
    using SimpleTypeWrapper<std::int16_t>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::Int32>
    : public SimpleTypeWrapper<std::int32_t>
{
    using SimpleTypeWrapper<std::int32_t>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::Int64>
    : public SimpleTypeWrapper<std::int64_t>
{
    using SimpleTypeWrapper<std::int64_t>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::Nothing> {
};

template <>
struct DataSourceType<DataSourceTypeId::String>
    : public SimpleTypeWrapper<std::string>
{
    using SimpleTypeWrapper<std::string>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::UInt8>
    : public SimpleTypeWrapper<std::uint8_t>
{
    using SimpleTypeWrapper<std::uint8_t>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::UInt16>
    : public SimpleTypeWrapper<std::uint16_t>
{
    using SimpleTypeWrapper<std::uint16_t>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::UInt32>
    : public SimpleTypeWrapper<std::uint32_t>
{
    using SimpleTypeWrapper<std::uint32_t>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::UInt64>
    : public SimpleTypeWrapper<std::uint64_t>
{
    using SimpleTypeWrapper<std::uint64_t>::SimpleTypeWrapper;
};

template <>
struct DataSourceType<DataSourceTypeId::UUID>
    : public SimpleTypeWrapper<SQLGUID>
{
    using SimpleTypeWrapper<SQLGUID>::SimpleTypeWrapper;
};

template <class T> struct is_string_data_source_type
    : public std::false_type
{
};

template <> struct is_string_data_source_type<DataSourceType<DataSourceTypeId::String>>
    : public std::true_type
{
};

template <> struct is_string_data_source_type<DataSourceType<DataSourceTypeId::FixedString>>
    : public std::true_type
{
};

template <> struct is_string_data_source_type<WireTypeAnyAsString>
    : public std::true_type
{
};

template <class T> inline constexpr bool is_string_data_source_type_v = is_string_data_source_type<T>::value;

// Used to avoid duplicate specializations in platforms where 'std::int32_t' or 'std::int64_t' are typedef'd as 'long'.
struct long_if_not_typedefed {
    struct dummy {};
    using type = std::conditional_t<
        (std::is_same_v<long, std::int32_t> || std::is_same_v<long, std::int64_t>),
        dummy,
        long
    >;
};

// Used to avoid duplicate specializations in platforms where 'std::uint32_t' or 'std::uint64_t' are typedef'd as 'unsigned long'.
struct unsigned_long_if_not_typedefed {
    struct dummy {};
    using type = std::conditional_t<
        (std::is_same_v<unsigned long, std::uint32_t> || std::is_same_v<unsigned long, std::uint64_t>),
        dummy,
        unsigned long
    >;
};

namespace value_manip {

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

    template <typename ProxyType, typename SourceType, typename DestinationType>
    void convert_via_proxy(const SourceType & src, DestinationType & dest);

    template <typename SourceType>
    struct from_value {
        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest); // Leave unimplemented for general case.
        };
    };

    template <>
    struct from_value<std::string> {
        using SourceType = std::string;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                dest = fromString<DestinationType>(src);
            }

            static inline void convert(SourceType && src, DestinationType & dest) {
                dest = fromString<DestinationType>(std::move(src));
            }
        };
    };

    template <>
    struct from_value<std::string>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            dest = src;
        }

        static inline void convert(SourceType && src, DestinationType & dest) {
            dest = std::move(src);
        }
    };

    template <>
    struct from_value<std::string>::to_value<std::int64_t> {
        using DestinationType = std::int64_t;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::size_t pos = 0;

            try {
                dest = std::stoll(src, &pos, 10);
            }
            catch (const std::exception & e) {
                throw std::runtime_error("Cannot interpret '" + src + "' as signed 64-bit integer: " + e.what());
            }

            if (pos != src.size())
                throw std::runtime_error("Cannot interpret '" + src + "' as signed 64-bit integer: string consumed partially");
        }
    };

    template <>
    struct from_value<std::string>::to_value<std::int32_t> {
        using DestinationType = std::int32_t;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::int64_t tmp = 0;
            to_value<std::int64_t>::convert(src, tmp);

            if (std::numeric_limits<DestinationType>::max() < tmp || tmp < std::numeric_limits<DestinationType>::min())
                throw std::runtime_error("Cannot interpret '" + src + "' as signed 32-bit integer: value out of range");

            dest = static_cast<std::int32_t>(tmp);
        }
    };

    template <>
    struct from_value<std::string>::to_value<long_if_not_typedefed::type> {
        using DestinationType = long;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::int64_t tmp = 0;
            to_value<std::int64_t>::convert(src, tmp);

            if (std::numeric_limits<DestinationType>::max() < tmp || tmp < std::numeric_limits<DestinationType>::min())
                throw std::runtime_error("Cannot interpret '" + src + "' as long integer: value out of range");

            dest = static_cast<long>(tmp);
        }
    };

    template <>
    struct from_value<std::string>::to_value<std::int16_t> {
        using DestinationType = std::int16_t;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::int64_t tmp = 0;
            to_value<std::int64_t>::convert(src, tmp);

            if (std::numeric_limits<DestinationType>::max() < tmp || tmp < std::numeric_limits<DestinationType>::min())
                throw std::runtime_error("Cannot interpret '" + src + "' as signed 16-bit integer: value out of range");

            dest = static_cast<std::int16_t>(tmp);
        }
    };

    template <>
    struct from_value<std::string>::to_value<std::int8_t> {
        using DestinationType = std::int8_t;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::int64_t tmp = 0;
            to_value<std::int64_t>::convert(src, tmp);

            if (std::numeric_limits<DestinationType>::max() < tmp || tmp < std::numeric_limits<DestinationType>::min())
                throw std::runtime_error("Cannot interpret '" + src + "' as signed 8-bit integer: value out of range");

            dest = static_cast<std::int8_t>(tmp);
        }
    };

    template <>
    struct from_value<std::string>::to_value<std::uint64_t> {
        using DestinationType = std::uint64_t;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::size_t pos = 0;

            try {
                dest = std::stoull(src, &pos, 10);
            }
            catch (const std::exception & e) {
                throw std::runtime_error("Cannot interpret '" + src + "' as unsigned 64-bit integer: " + e.what());
            }

            if (pos != src.size())
                throw std::runtime_error("Cannot interpret '" + src + "' as unsigned 64-bit integer: string consumed partially");
        }
    };

    template <>
    struct from_value<std::string>::to_value<std::uint32_t> {
        using DestinationType = std::uint32_t;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::uint64_t tmp = 0;
            to_value<std::uint64_t>::convert(src, tmp);

            if (std::numeric_limits<DestinationType>::max() < tmp || tmp < std::numeric_limits<DestinationType>::min())
                throw std::runtime_error("Cannot interpret '" + src + "' as unsigned 32-bit integer: value out of range");

            dest = static_cast<std::uint32_t>(tmp);
        }
    };

    template <>
    struct from_value<std::string>::to_value<unsigned_long_if_not_typedefed::type> {
        using DestinationType = unsigned long;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::uint64_t tmp = 0;
            to_value<std::uint64_t>::convert(src, tmp);

            if (std::numeric_limits<DestinationType>::max() < tmp || tmp < std::numeric_limits<DestinationType>::min())
                throw std::runtime_error("Cannot interpret '" + src + "' as unsigned long integer: value out of range");

            dest = static_cast<unsigned long>(tmp);
        }
    };

    template <>
    struct from_value<std::string>::to_value<std::uint16_t> {
        using DestinationType = std::uint16_t;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::uint64_t tmp = 0;
            to_value<std::uint64_t>::convert(src, tmp);

            if (std::numeric_limits<DestinationType>::max() < tmp || tmp < std::numeric_limits<DestinationType>::min())
                throw std::runtime_error("Cannot interpret '" + src + "' as unsigned 16-bit integer: value out of range");

            dest = static_cast<std::uint16_t>(tmp);
        }
    };

    template <>
    struct from_value<std::string>::to_value<std::uint8_t> {
        using DestinationType = std::uint8_t;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::uint64_t tmp = 0;
            to_value<std::uint64_t>::convert(src, tmp);

            if (std::numeric_limits<DestinationType>::max() < tmp || tmp < std::numeric_limits<DestinationType>::min())
                throw std::runtime_error("Cannot interpret '" + src + "' as unsigned 8-bit integer: value out of range");

            dest = static_cast<std::uint8_t>(tmp);
        }
    };

    template <>
    struct from_value<std::string>::to_value<float> {
        using DestinationType = float;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::size_t pos = 0;

            try {
                dest = std::stof(src, &pos);
            }
            catch (const std::exception & e) {
                throw std::runtime_error("Cannot interpret '" + src + "' as float: " + e.what());
            }

            if (pos != src.size())
                throw std::runtime_error("Cannot interpret '" + src + "' as float: string consumed partially");
        }
    };

    template <>
    struct from_value<std::string>::to_value<double> {
        using DestinationType = double;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::size_t pos = 0;

            try {
                dest = std::stod(src, &pos);
            }
            catch (const std::exception & e) {
                throw std::runtime_error("Cannot interpret '" + src + "' as double: " + e.what());
            }

            if (pos != src.size())
                throw std::runtime_error("Cannot interpret '" + src + "' as double: string consumed partially");
        }
    };

    template <>
    struct from_value<std::string>::to_value<SQLGUID> {
        using DestinationType = SQLGUID;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            unsigned int Data1 = 0;
            unsigned int Data2 = 0;
            unsigned int Data3 = 0;
            unsigned int Data4[8] = { 0 };
            char guard = '\0';

            const auto read = std::sscanf(src.c_str(), "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x%c",
                &Data1, &Data2, &Data3,
                &Data4[0], &Data4[1], &Data4[2], &Data4[3],
                &Data4[4], &Data4[5], &Data4[6], &Data4[7],
                &guard
            );

            if (read != 11) // All 'DataN' must be successfully read, but not the 'guard'.
                throw std::runtime_error("Cannot interpret '" + src + "' as GUID");

            dest.Data1 = Data1;
            dest.Data2 = Data2;
            dest.Data3 = Data3;
            std::copy(std::begin(Data4), std::end(Data4), std::begin(dest.Data4));
        }
    };

    template <>
    struct from_value<std::string>::to_value<SQL_NUMERIC_STRUCT> {
        using DestinationType = SQL_NUMERIC_STRUCT;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            convert_via_proxy<DataSourceType<DataSourceTypeId::Decimal>>(src, dest);
        }
    };

    template <>
    struct from_value<std::string>::to_value<SQL_DATE_STRUCT> {
        using DestinationType = SQL_DATE_STRUCT;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            if (src.size() != 10)
                throw std::runtime_error("Cannot interpret '" + src + "' as Date");

            dest.year = (src[0] - '0') * 1000 + (src[1] - '0') * 100 + (src[2] - '0') * 10 + (src[3] - '0');
            dest.month = (src[5] - '0') * 10 + (src[6] - '0');
            dest.day = (src[8] - '0') * 10 + (src[9] - '0');

            normalize_date(dest);
        }
    };

    template <>
    struct from_value<std::string>::to_value<SQL_TIME_STRUCT> {
        using DestinationType = SQL_TIME_STRUCT;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            if constexpr (std::is_same_v<SourceType, DestinationType>) {
                std::memcpy(&dest, &src, sizeof(dest));
            }
            else {
                throw std::runtime_error("conversion not supported");

                // TODO: implement?

            }
        }
    };

    template <>
    struct from_value<std::string>::to_value<SQL_TIMESTAMP_STRUCT> {
        using DestinationType = SQL_TIMESTAMP_STRUCT;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            if (src.size() == 10) {
                dest.year = (src[0] - '0') * 1000 + (src[1] - '0') * 100 + (src[2] - '0') * 10 + (src[3] - '0');
                dest.month = (src[5] - '0') * 10 + (src[6] - '0');
                dest.day = (src[8] - '0') * 10 + (src[9] - '0');
                dest.hour = 0;
                dest.minute = 0;
                dest.second = 0;
                dest.fraction = 0;
            }
            else if (src.size() == 19) {
                dest.year = (src[0] - '0') * 1000 + (src[1] - '0') * 100 + (src[2] - '0') * 10 + (src[3] - '0');
                dest.month = (src[5] - '0') * 10 + (src[6] - '0');
                dest.day = (src[8] - '0') * 10 + (src[9] - '0');
                dest.hour = (src[11] - '0') * 10 + (src[12] - '0');
                dest.minute = (src[14] - '0') * 10 + (src[15] - '0');
                dest.second = (src[17] - '0') * 10 + (src[18] - '0');
                dest.fraction = 0;
            }
            else
                throw std::runtime_error("Cannot interpret '" + src + "' as DateTime");

            normalize_date(dest);
        }
    };

    template <DataSourceTypeId Id>
    struct from_value<std::string>::to_value<DataSourceType<Id>> {
        using DestinationType = DataSourceType<Id>;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            return from_value<SourceType>::template to_value<decltype(dest.value)>::convert(src, dest.value);
        }

        static inline void convert(SourceType && src, DestinationType & dest) {
            return from_value<SourceType>::template to_value<decltype(dest.value)>::convert(std::move(src), dest.value);
        }
    };

    template <>
    struct from_value<std::string>::to_value<DataSourceType<DataSourceTypeId::Decimal>> {
        using DestinationType = DataSourceType<DataSourceTypeId::Decimal>;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            constexpr auto dest_value_max = (std::numeric_limits<std::decay_t<decltype(dest.value)>>::max)();
            constexpr std::uint32_t dec_mult = 10;

            std::size_t left_n = 0;
            std::size_t right_n = 0;
            bool sign_met = false;
            bool dot_met = false;
            bool dig_met = false;

            dest.value = 0;
            dest.sign = 1;

            for (auto ch : src) {
                switch (ch) {
                    case '+':
                    case '-': {
                        if (sign_met || dot_met || dig_met)
                            throw std::runtime_error("Cannot interpret '" + src + "' as Decimal/Numeric");

                        if (ch == '-')
                            dest.sign = 0;

                        sign_met = true;
                        break;
                    }

                    case '.': {
                        if (dot_met)
                            throw std::runtime_error("Cannot interpret '" + src + "' as Decimal/Numeric");

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

                        if (dest.value != 0) {
                            if ((dest_value_max / dec_mult) < dest.value)
                                throw std::runtime_error("Cannot interpret '" + src + "' as Decimal/Numeric: value is too big for internal representation");
                            dest.value *= dec_mult;
                        }

                        if (next_dec_dig != 0) {
                            if ((dest_value_max - next_dec_dig) < dest.value)
                                throw std::runtime_error("Cannot interpret '" + src + "' as Decimal/Numeric: value is too big for internal representation");
                            dest.value += next_dec_dig;
                        }

                        if (dot_met)
                            ++right_n;
                        else
                            ++left_n;

                        dig_met = true;
                        break;
                    }

                    default:
                        throw std::runtime_error("Cannot interpret '" + src + "' as Decimal/Numeric");
                }
            }

            if (dest.value == 0)
                dest.sign = 1;

            dest.precision = left_n + right_n;
            dest.scale = right_n;
        }
    };

    template <>
    struct from_value<std::string>::to_value<DataSourceType<DataSourceTypeId::Decimal32>> {
        using DestinationType = DataSourceType<DataSourceTypeId::Decimal32>;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            return to_value<DataSourceType<DataSourceTypeId::Decimal>>::convert(src, dest);
        }
    };

    template <>
    struct from_value<std::string>::to_value<DataSourceType<DataSourceTypeId::Decimal64>> {
        using DestinationType = DataSourceType<DataSourceTypeId::Decimal64>;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            return to_value<DataSourceType<DataSourceTypeId::Decimal>>::convert(src, dest);
        }
    };

    template <>
    struct from_value<std::string>::to_value<DataSourceType<DataSourceTypeId::Decimal128>> {
        using DestinationType = DataSourceType<DataSourceTypeId::Decimal128>;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            return to_value<DataSourceType<DataSourceTypeId::Decimal>>::convert(src, dest);
        }
    };

    template <>
    struct from_value<std::int64_t> {
        using SourceType = std::int64_t;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::string>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<std::int64_t>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            dest = std::to_string(src);
        }
    };

    template <>
    struct from_value<std::int32_t> {
        using SourceType = std::int32_t;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::int64_t>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<long_if_not_typedefed::type> {
        using SourceType = long;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::int64_t>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<std::int16_t> {
        using SourceType = std::int16_t;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::int64_t>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<std::int8_t> {
        using SourceType = std::int8_t;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::int64_t>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<std::uint64_t> {
        using SourceType = std::uint64_t;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::string>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<std::uint64_t>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            dest = std::to_string(src);
        }
    };

    template <>
    struct from_value<std::uint32_t> {
        using SourceType = std::uint32_t;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::uint64_t>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<unsigned_long_if_not_typedefed::type> {
        using SourceType = unsigned long;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::uint64_t>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<std::uint16_t> {
        using SourceType = std::uint16_t;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::uint64_t>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<std::uint8_t> {
        using SourceType = std::uint8_t;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::uint64_t>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<float> {
        using SourceType = float;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::string>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<float>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            dest = std::to_string(src);
        }
    };

    template <>
    struct from_value<double> {
        using SourceType = double;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_convertible_v<SourceType, DestinationType>) {
                    dest = src;
                }
                else {
                    convert_via_proxy<std::string>(src, dest);
                }
            }
        };
    };

    template <>
    struct from_value<double>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            dest = std::to_string(src);
        }
    };

    template <>
    struct from_value<SQLCHAR *> {
        using SourceType = SQLCHAR *;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                convert_via_proxy<std::string>(src, dest);
            }
        };
    };

    template <>
    struct from_value<SQLCHAR *>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            dest = toUTF8(src);
        }
    };

    template <>
    struct from_value<SQLWCHAR *> {
        using SourceType = SQLWCHAR *;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                convert_via_proxy<std::string>(src, dest);
            }
        };
    };

    template <>
    struct from_value<SQLWCHAR *>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            dest = toUTF8(src);
        }
    };

    template <>
    struct from_value<SQLGUID> {
        using SourceType = SQLGUID;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_same_v<SourceType, DestinationType>) {
                    std::memcpy(&dest, &src, sizeof(dest));
                }
                else {
                    throw std::runtime_error("conversion not supported");
                }
            }
        };
    };

    template <>
    struct from_value<SQLGUID>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            char buf[256];

            const auto written = std::snprintf(buf, lengthof(buf), "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                (unsigned int)src.Data1,    (unsigned int)src.Data2,    (unsigned int)src.Data3,
                (unsigned int)src.Data4[0], (unsigned int)src.Data4[1], (unsigned int)src.Data4[2], (unsigned int)src.Data4[3],
                (unsigned int)src.Data4[4], (unsigned int)src.Data4[5], (unsigned int)src.Data4[6], (unsigned int)src.Data4[7]
            );

            if (written < 36 || written >= lengthof(buf))
                buf[0] = '\0';

            dest = buf;
        }
    };

    template <>
    struct from_value<SQL_NUMERIC_STRUCT> {
        using SourceType = SQL_NUMERIC_STRUCT;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_same_v<SourceType, DestinationType>) {
                    if (src.precision == dest.precision && src.scale == dest.scale) {
                        std::memcpy(&dest, &src, sizeof(dest));
                    }
                    else {
                        convert_via_proxy<DataSourceType<DataSourceTypeId::Decimal>>(src, dest);
                    }
                }
                else {
                    throw std::runtime_error("conversion not supported");
                }
            }
        };
    };

    template <>
    struct from_value<SQL_NUMERIC_STRUCT>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            convert_via_proxy<DataSourceType<DataSourceTypeId::Decimal>>(src, dest);
        }
    };

    template <>
    struct from_value<SQL_NUMERIC_STRUCT>::to_value<DataSourceType<DataSourceTypeId::Decimal>> {
        using DestinationType = DataSourceType<DataSourceTypeId::Decimal>;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            dest.sign = src.sign;
            dest.precision = src.precision;
            dest.scale = src.scale;

            constexpr auto dest_max = (std::numeric_limits<decltype(dest.value)>::max)();
            constexpr std::uint32_t byte_mult = 1 << 8;

            for (std::size_t i = 1; i <= lengthof(src.val); ++i) {
                const std::uint32_t next_byte_dig = static_cast<unsigned char>(src.val[lengthof(src.val) - i]);

                if (dest.value != 0) {
                    if ((dest_max / byte_mult) < dest.value)
                        throw std::runtime_error("Numeric value is too big for internal representation");
                    dest.value *= byte_mult;
                }

                if (next_byte_dig != 0) {
                    if ((dest_max - next_byte_dig) < dest.value)
                        throw std::runtime_error("Numeric value is too big for internal representation");
                    dest.value += next_byte_dig;
                }
            }
        }
    };

    template <>
    struct from_value<SQL_DATE_STRUCT> {
        using SourceType = SQL_DATE_STRUCT;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_same_v<SourceType, DestinationType>) {
                    std::memcpy(&dest, &src, sizeof(dest));
                }
                else {
                    throw std::runtime_error("conversion not supported");
                }
            }
        };
    };

    template <>
    struct from_value<SQL_DATE_STRUCT>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            char buf[256];

            const auto written = std::snprintf(buf, lengthof(buf), "%04d-%02d-%02d", (int)src.year, (int)src.month, (int)src.day);
            if (written < 10 || written >= lengthof(buf))
                buf[0] = '\0';

            dest = buf;
        }
    };

    template <>
    struct from_value<SQL_TIME_STRUCT> {
        using SourceType = SQL_TIME_STRUCT;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_same_v<SourceType, DestinationType>) {
                    std::memcpy(&dest, &src, sizeof(dest));
                }
                else {
                    throw std::runtime_error("conversion not supported");
                }
            }
        };
    };

    template <>
    struct from_value<SQL_TIME_STRUCT>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            char buf[256];

            const auto written = std::snprintf(buf, lengthof(buf), "%02d:%02d:%02d", (int)src.hour, (int)src.minute, (int)src.second);
            if (written < 8 || written >= lengthof(buf))
                buf[0] = '\0';

            dest = buf;
        }
    };

    template <>
    struct from_value<SQL_TIMESTAMP_STRUCT> {
        using SourceType = SQL_TIMESTAMP_STRUCT;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                if constexpr (std::is_same_v<SourceType, DestinationType>) {
                    std::memcpy(&dest, &src, sizeof(dest));
                }
                else {
                    throw std::runtime_error("conversion not supported");
                }
            }
        };
    };

    template <>
    struct from_value<SQL_TIMESTAMP_STRUCT>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            char buf[256];

            const auto written = std::snprintf(buf, lengthof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                    (int)src.year, (int)src.month, (int)src.day,
                    (int)src.hour, (int)src.minute, (int)src.second
            );

            if (written < 8 || written >= lengthof(buf)) {
                buf[0] = '\0';
            }
            else if (src.fraction > 0 && src.fraction < 1000000000) {
                const auto written_more = std::snprintf(buf + written, lengthof(buf) - written, ".%09d", (int)src.fraction);
                if (written_more < 2 || written_more >= (lengthof(buf) - written))
                    buf[written] = '\0';
            }

            dest = buf;
        }
    };

    template <DataSourceTypeId Id>
    struct from_value<DataSourceType<Id>> {
        using SourceType = DataSourceType<Id>;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                return from_value<decltype(src.value)>::template to_value<DestinationType>::convert(src.value, dest);
            }

            static inline void convert(SourceType && src, DestinationType & dest) {
                return from_value<decltype(src.value)>::template to_value<DestinationType>::convert(std::move(src.value), dest); // Not all DataSourceType<> have .value though...
            }
        };
    };

    template <>
    struct from_value<DataSourceType<DataSourceTypeId::Nothing>> {
        using SourceType = DataSourceType<DataSourceTypeId::Nothing>;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                to_null(dest);
            }
        };
    };

    template <>
    struct from_value<DataSourceType<DataSourceTypeId::Decimal>> {
        using SourceType = DataSourceType<DataSourceTypeId::Decimal>;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                throw std::runtime_error("conversion not supported");
            }
        };
    };

    template <>
    struct from_value<DataSourceType<DataSourceTypeId::Decimal>>::to_value<std::string> {
        using DestinationType = std::string;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            dest.reserve(128);

            auto tmp_value = src.value;
            constexpr std::uint32_t dec_mult = 10;

            while (tmp_value != 0 || dest.size() < src.scale) {
                char next_dig = '0';

                if (tmp_value != 0) {
                    next_dig += tmp_value % dec_mult;
                    tmp_value /= dec_mult;
                }

                dest.push_back(next_dig);

                if (dest.size() == src.scale)
                    dest.push_back('.');
            }

            if (dest.empty())
                dest.push_back('0');
            else if (src.sign == 0 && src.value != 0)
                dest.push_back('-');

            std::reverse(dest.begin(), dest.end());
        }
    };

    template <>
    struct from_value<DataSourceType<DataSourceTypeId::Decimal>>::to_value<SQL_NUMERIC_STRUCT> {
        using DestinationType = SQL_NUMERIC_STRUCT;

        static inline void convert(const SourceType & src, DestinationType & dest) {

            // Using the target precision and scale from dest.
            // If dest.precision == 0 then the src.precision and src.scale will be used.
            // Otherwise, the requested dest.precision and dest.scale will be enforced.

            if (dest.precision < 0 || dest.precision < dest.scale)
                throw std::runtime_error("Bad Numeric specification");

            constexpr auto src_value_max = (std::numeric_limits<decltype(src.value)>::max)();
            constexpr std::uint32_t dec_mult = 10;
            constexpr std::uint32_t byte_mult = 1 << 8;

            dest.sign = src.sign;

            if (dest.precision == 0) {
                dest.precision = src.precision;
                dest.scale = src.scale;
            }

            auto tmp_src = src;

            // Adjust the detected scale if needed.

            while (tmp_src.scale < dest.scale) {
                if ((src_value_max / dec_mult) < tmp_src.value)
                    throw std::runtime_error("Cannot fit source Numeric value into destination Numeric specification: value is too big for internal representation");

                tmp_src.value *= dec_mult;
                ++tmp_src.scale;
            }

            while (dest.scale < tmp_src.scale) {
                tmp_src.value /= dec_mult;
                --tmp_src.scale;
            }

            // Transfer the value.

            for (std::size_t i = 0; tmp_src.value != 0; ++i) {
                if (i >= lengthof(dest.val) || i > dest.precision)
                    throw std::runtime_error("Cannot fit source Numeric value into destination Numeric specification: value is too big for ODBC Numeric representation");

                dest.val[i] = static_cast<std::uint32_t>(tmp_src.value % byte_mult);
                tmp_src.value /= byte_mult;
            }
        }
    };

    template <>
    struct from_value<DataSourceType<DataSourceTypeId::Decimal32>> {
        using SourceType = DataSourceType<DataSourceTypeId::Decimal32>;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                return from_value<DataSourceType<DataSourceTypeId::Decimal>>::template to_value<DestinationType>::convert(src, dest);
            }
        };
    };

    template <>
    struct from_value<DataSourceType<DataSourceTypeId::Decimal64>> {
        using SourceType = DataSourceType<DataSourceTypeId::Decimal64>;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                return from_value<DataSourceType<DataSourceTypeId::Decimal>>::template to_value<DestinationType>::convert(src, dest);
            }
        };
    };

    template <>
    struct from_value<DataSourceType<DataSourceTypeId::Decimal128>> {
        using SourceType = DataSourceType<DataSourceTypeId::Decimal128>;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                return from_value<DataSourceType<DataSourceTypeId::Decimal>>::template to_value<DestinationType>::convert(src, dest);
            }
        };
    };

    template <>
    struct from_value<WireTypeAnyAsString> {
        using SourceType = WireTypeAnyAsString;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                return from_value<std::string>::template to_value<DestinationType>::convert(src.value, dest);
            }
        };
    };

    template <>
    struct from_value<WireTypeDateAsInt> {
        using SourceType = WireTypeDateAsInt;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                convert_via_proxy<DataSourceType<DataSourceTypeId::Date>>(src, dest);
            }
        };
    };

    template <>
    struct from_value<WireTypeDateAsInt>::to_value<DataSourceType<DataSourceTypeId::Date>> {
        using DestinationType = DataSourceType<DataSourceTypeId::Date>;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::time_t time = src.value;
            time = time * 24 * 60 * 60; // Now it's seconds since epoch.
            const auto & tm = *std::localtime(&time);

            dest.value.year = 1900 + tm.tm_year;
            dest.value.month = 1 + tm.tm_mon;
            dest.value.day = tm.tm_mday;
        }
    };

    template <>
    struct from_value<WireTypeDateTimeAsInt> {
        using SourceType = WireTypeDateTimeAsInt;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const SourceType & src, DestinationType & dest) {
                convert_via_proxy<DataSourceType<DataSourceTypeId::DateTime>>(src, dest);
            }
        };
    };

    template <>
    struct from_value<WireTypeDateTimeAsInt>::to_value<DataSourceType<DataSourceTypeId::DateTime>> {
        using DestinationType = DataSourceType<DataSourceTypeId::DateTime>;

        static inline void convert(const SourceType & src, DestinationType & dest) {
            std::time_t time = src.value;
            const auto & tm = *std::localtime(&time);

            dest.value.year = 1900 + tm.tm_year;
            dest.value.month = 1 + tm.tm_mon;
            dest.value.day = tm.tm_mday;
            dest.value.hour = tm.tm_hour;
            dest.value.minute = tm.tm_min;
            dest.value.second = tm.tm_sec;
            dest.value.fraction = 0;
        }
    };

    template <typename DestinationType>
    struct to_buffer {
        template <typename SourceType>
        struct from_value {
            static inline SQLRETURN convert(const SourceType & src, BindingInfo & dest) {
                if (dest.indicator && dest.indicator != dest.value_size)
                    *dest.indicator = 0; // (Null) indicator pointer of the binding. Value is not null here so we store 0 in it.

                if constexpr (std::is_same_v<SourceType, DestinationType>) {
                    return fillOutputPOD(src, dest.value, dest.value_size);
                }
                else {
                    DestinationType dest_obj;
                    to_null(dest_obj);
                    ::value_manip::from_value<SourceType>::template to_value<DestinationType>::convert(src, dest_obj);
                    return fillOutputPOD(dest_obj, dest.value, dest.value_size);
                }
            }
        };
    };

    template <>
    struct to_buffer<SQLCHAR *> {
        using DestinationType = SQLCHAR *;

        template <typename SourceType>
        struct from_value {
            template <typename ConversionContext>
            static inline SQLRETURN convert(const SourceType & src, BindingInfo & dest, ConversionContext && context) {
                if (dest.indicator && dest.indicator != dest.value_size)
                    *dest.indicator = 0; // (Null) indicator pointer of the binding. Value is not null here so we store 0 in it.

                if constexpr (std::is_same_v<SourceType, std::string>) {
                    return fillOutputString<SQLCHAR>(src, dest.value, dest.value_max_size, dest.value_size, true, std::forward<ConversionContext>(context));
                }
                else if constexpr (is_string_data_source_type_v<SourceType>) {
                    return fillOutputString<SQLCHAR>(src.value, dest.value, dest.value_max_size, dest.value_size, true, std::forward<ConversionContext>(context));
                }
                else {
                    std::string dest_obj;
                    to_null(dest_obj);
                    ::value_manip::from_value<SourceType>::template to_value<std::string>::convert(src, dest_obj);
                    return fillOutputString<SQLCHAR>(dest_obj, dest.value, dest.value_max_size, dest.value_size, true, std::forward<ConversionContext>(context));
                }
            }
        };
    };

    template <>
    struct to_buffer<SQLWCHAR *> {
        using DestinationType = SQLWCHAR *;

        template <typename SourceType>
        struct from_value {
            template <typename ConversionContext>
            static inline SQLRETURN convert(const SourceType & src, BindingInfo & dest, ConversionContext && context) {
                if (dest.indicator && dest.indicator != dest.value_size)
                    *dest.indicator = 0; // (Null) indicator pointer of the binding. Value is not null here so we store 0 in it.

                if constexpr (std::is_same_v<SourceType, std::string>) {
                    return fillOutputString<SQLWCHAR>(src, dest.value, dest.value_max_size, dest.value_size, true, std::forward<ConversionContext>(context));
                }
                else if constexpr (is_string_data_source_type_v<SourceType>) {
                    return fillOutputString<SQLWCHAR>(src.value, dest.value, dest.value_max_size, dest.value_size, true, std::forward<ConversionContext>(context));
                }
                else {
                    std::string dest_obj;
                    to_null(dest_obj);
                    ::value_manip::from_value<SourceType>::template to_value<std::string>::convert(src, dest_obj);
                    return fillOutputString<SQLWCHAR>(dest_obj, dest.value, dest.value_max_size, dest.value_size, true, std::forward<ConversionContext>(context));
                }
            }
        };
    };

    template <>
    struct to_buffer<SQL_NUMERIC_STRUCT> {
        using DestinationType = SQL_NUMERIC_STRUCT;

        template <typename SourceType>
        struct from_value {
            static inline SQLRETURN convert(const SourceType & src, BindingInfo & dest) {
                if (dest.indicator && dest.indicator != dest.value_size)
                    *dest.indicator = 0; // (Null) indicator pointer of the binding. Value is not null here so we store 0 in it.

                if constexpr (std::is_same_v<SourceType, DestinationType>) {
                    if (
                        src.precision == dest.precision &&
                        src.scale == dest.scale
                    ) {
                        return fillOutputPOD(src, dest.value, dest.value_size);
                    }
                }

                SQL_NUMERIC_STRUCT dest_obj;
                to_null(dest_obj);

                dest_obj.precision = dest.precision;
                dest_obj.scale = dest.scale;

                ::value_manip::from_value<SourceType>::template to_value<DestinationType>::convert(src, dest_obj);

                return fillOutputPOD(dest_obj, dest.value, dest.value_size);
            }
        };
    };

    template <typename SourceType>
    struct from_buffer {
        template <typename DestinationType>
        struct to_value {
            static inline void convert(const BindingInfo & src, DestinationType & dest) {
                if (!src.value) {
                    to_null(dest);
                    return;
                }

                const auto * ind_ptr = src.indicator;

                if (ind_ptr) {
                    switch (*ind_ptr) {
                        case 0:
                        case SQL_NTS: {
                            break;
                        }

                        case SQL_NULL_DATA: {
                            to_null(dest);
                            return;
                        }

                        case SQL_DEFAULT_PARAM: {
                            to_default(dest);
                            return;
                        }

                        default: {
                            if (*ind_ptr == SQL_DATA_AT_EXEC || *ind_ptr < 0)
                                throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
                            break;
                        }
                    }
                }

                const auto & src_obj = *reinterpret_cast<SourceType *>(src.value);
                ::value_manip::from_value<SourceType>::template to_value<DestinationType>::convert(src_obj, dest);
            }
        };
    };

    template <>
    struct from_buffer<SQLCHAR *> {
        using SourceType = SQLCHAR *;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const BindingInfo & src, DestinationType & dest) {
                if (!src.value) {
                    to_null(dest);
                    return;
                }

                const auto * cstr = reinterpret_cast<const SQLCHAR *>(src.value);
                const auto * sz_ptr = src.value_size;
                const auto * ind_ptr = src.indicator;

                if (ind_ptr) {
                    switch (*ind_ptr) {
                        case 0:
                        case SQL_NTS: {
                            ::value_manip::from_value<std::string>::template to_value<DestinationType>::convert(toUTF8(cstr), dest);
                            return;
                        }

                        case SQL_NULL_DATA: {
                            to_null(dest);
                            return;
                        }

                        case SQL_DEFAULT_PARAM: {
                            to_default(dest);
                            return;
                        }

                        default: {
                            if (*ind_ptr == SQL_DATA_AT_EXEC || *ind_ptr < 0)
                                throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
                            break;
                        }
                    }
                }

                if (!sz_ptr || *sz_ptr < 0) {
                    ::value_manip::from_value<std::string>::template to_value<DestinationType>::convert(toUTF8(cstr), dest);
                }
                else {
                    ::value_manip::from_value<std::string>::template to_value<DestinationType>::convert(
                        toUTF8(cstr, (static_cast<std::size_t>(*sz_ptr) / sizeof(decltype(*cstr)))), dest);
                }
            }
        };
    };

    template <>
    struct from_buffer<SQLWCHAR *> {
        using SourceType = SQLWCHAR *;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const BindingInfo & src, DestinationType & dest) {
                if (!src.value) {
                    to_null(dest);
                    return;
                }

                const auto * cstr = reinterpret_cast<const SQLWCHAR *>(src.value);
                const auto * sz_ptr = src.value_size;
                const auto * ind_ptr = src.indicator;

                if (ind_ptr) {
                    switch (*ind_ptr) {
                        case 0:
                        case SQL_NTS: {
                            ::value_manip::from_value<std::string>::template to_value<DestinationType>::convert(toUTF8(cstr), dest);
                            return;
                        }

                        case SQL_NULL_DATA: {
                            to_null(dest);
                            return;
                        }

                        case SQL_DEFAULT_PARAM: {
                            to_default(dest);
                            return;
                        }

                        default: {
                            if (*ind_ptr == SQL_DATA_AT_EXEC || *ind_ptr < 0)
                                throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
                            break;
                        }
                    }
                }

                if (!sz_ptr || *sz_ptr < 0) {
                    ::value_manip::from_value<std::string>::template to_value<DestinationType>::convert(toUTF8(cstr), dest);
                }
                else {
                    ::value_manip::from_value<std::string>::template to_value<DestinationType>::convert(
                        toUTF8(cstr, (static_cast<std::size_t>(*sz_ptr) / sizeof(decltype(*cstr)))), dest);
                }
            }
        };
    };

    template <>
    struct from_buffer<SQL_NUMERIC_STRUCT> {
        using SourceType = SQL_NUMERIC_STRUCT;

        template <typename DestinationType>
        struct to_value {
            static inline void convert(const BindingInfo & src, DestinationType & dest) {
                if (!src.value) {
                    to_null(dest);
                    return;
                }

                const auto * ind_ptr = src.indicator;

                if (ind_ptr) {
                    switch (*ind_ptr) {
                        case 0:
                        case SQL_NTS: {
                            break;
                        }

                        case SQL_NULL_DATA: {
                            to_null(dest);
                            return;
                        }

                        case SQL_DEFAULT_PARAM: {
                            to_default(dest);
                            return;
                        }

                        default: {
                            if (*ind_ptr == SQL_DATA_AT_EXEC || *ind_ptr < 0)
                                throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
                            break;
                        }
                    }
                }

                const auto & src_obj = *reinterpret_cast<SQL_NUMERIC_STRUCT *>(src.value);

                SQL_NUMERIC_STRUCT tmp_src_obj;
                std::memcpy(&tmp_src_obj, &src_obj, sizeof(tmp_src_obj));

                tmp_src_obj.precision = src.precision;
                tmp_src_obj.scale = src.scale;

                ::value_manip::from_value<SourceType>::template to_value<DestinationType>::convert(tmp_src_obj, dest);
            }
        };
    };

    template <typename ProxyType, typename SourceType, typename DestinationType>
    void convert_via_proxy(const SourceType & src, DestinationType & dest) {
        ProxyType tmp_src;
        to_null(tmp_src);
        from_value<SourceType>::template to_value<ProxyType>::convert(src, tmp_src);
        from_value<ProxyType>::template to_value<DestinationType>::convert(tmp_src, dest);
    }

} // namespace value_manip

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
inline auto readReadyDataTo(const BindingInfo & src, T & dest) {
    switch (src.c_type) {
        case SQL_C_CHAR:           return value_manip::from_buffer< SQLCHAR *            >::template to_value< T >::convert(src, dest);
        case SQL_C_WCHAR:          return value_manip::from_buffer< SQLWCHAR *           >::template to_value< T >::convert(src, dest);
        case SQL_C_BIT:            return value_manip::from_buffer< SQLCHAR              >::template to_value< T >::convert(src, dest);
        case SQL_C_TINYINT:        return value_manip::from_buffer< SQLSCHAR             >::template to_value< T >::convert(src, dest);
        case SQL_C_STINYINT:       return value_manip::from_buffer< SQLSCHAR             >::template to_value< T >::convert(src, dest);
        case SQL_C_UTINYINT:       return value_manip::from_buffer< SQLCHAR              >::template to_value< T >::convert(src, dest);
        case SQL_C_SHORT:          return value_manip::from_buffer< SQLSMALLINT          >::template to_value< T >::convert(src, dest);
        case SQL_C_SSHORT:         return value_manip::from_buffer< SQLSMALLINT          >::template to_value< T >::convert(src, dest);
        case SQL_C_USHORT:         return value_manip::from_buffer< SQLUSMALLINT         >::template to_value< T >::convert(src, dest);
        case SQL_C_LONG:           return value_manip::from_buffer< SQLINTEGER           >::template to_value< T >::convert(src, dest);
        case SQL_C_SLONG:          return value_manip::from_buffer< SQLINTEGER           >::template to_value< T >::convert(src, dest);
        case SQL_C_ULONG:          return value_manip::from_buffer< SQLUINTEGER          >::template to_value< T >::convert(src, dest);
        case SQL_C_SBIGINT:        return value_manip::from_buffer< SQLBIGINT            >::template to_value< T >::convert(src, dest);
        case SQL_C_UBIGINT:        return value_manip::from_buffer< SQLUBIGINT           >::template to_value< T >::convert(src, dest);
        case SQL_C_FLOAT:          return value_manip::from_buffer< SQLREAL              >::template to_value< T >::convert(src, dest);
        case SQL_C_DOUBLE:         return value_manip::from_buffer< SQLDOUBLE            >::template to_value< T >::convert(src, dest);
        case SQL_C_BINARY:         return value_manip::from_buffer< SQLCHAR *            >::template to_value< T >::convert(src, dest);
        case SQL_C_GUID:           return value_manip::from_buffer< SQLGUID              >::template to_value< T >::convert(src, dest);

//      case SQL_C_BOOKMARK:       return value_manip::from_buffer< BOOKMARK             >::template to_value< T >::convert(src, dest);
//      case SQL_C_VARBOOKMARK:    return value_manip::from_buffer< SQLCHAR *            >::template to_value< T >::convert(src, dest);

        case SQL_C_NUMERIC:        return value_manip::from_buffer< SQL_NUMERIC_STRUCT   >::template to_value< T >::convert(src, dest);

        case SQL_C_DATE:
        case SQL_C_TYPE_DATE:      return value_manip::from_buffer< SQL_DATE_STRUCT      >::template to_value< T >::convert(src, dest);

        case SQL_C_TIME:
        case SQL_C_TYPE_TIME:      return value_manip::from_buffer< SQL_TIME_STRUCT      >::template to_value< T >::convert(src, dest);

        case SQL_C_TIMESTAMP:
        case SQL_C_TYPE_TIMESTAMP: return value_manip::from_buffer< SQL_TIMESTAMP_STRUCT >::template to_value< T >::convert(src, dest);

        default:
            throw std::runtime_error("Unable to extract data from bound buffer: source type representation not supported");
    }
}

template <typename T, typename ConversionContext>
inline auto writeDataFrom(const T & src, BindingInfo & dest, ConversionContext && context) {
    switch (dest.c_type) {
        case SQL_C_CHAR:           return value_manip::to_buffer< SQLCHAR *            >::template from_value< T >::convert(src, dest, std::forward<ConversionContext>(context));
        case SQL_C_WCHAR:          return value_manip::to_buffer< SQLWCHAR *           >::template from_value< T >::convert(src, dest, std::forward<ConversionContext>(context));
        case SQL_C_BIT:            return value_manip::to_buffer< SQLCHAR              >::template from_value< T >::convert(src, dest);
        case SQL_C_TINYINT:        return value_manip::to_buffer< SQLSCHAR             >::template from_value< T >::convert(src, dest);
        case SQL_C_STINYINT:       return value_manip::to_buffer< SQLSCHAR             >::template from_value< T >::convert(src, dest);
        case SQL_C_UTINYINT:       return value_manip::to_buffer< SQLCHAR              >::template from_value< T >::convert(src, dest);
        case SQL_C_SHORT:          return value_manip::to_buffer< SQLSMALLINT          >::template from_value< T >::convert(src, dest);
        case SQL_C_SSHORT:         return value_manip::to_buffer< SQLSMALLINT          >::template from_value< T >::convert(src, dest);
        case SQL_C_USHORT:         return value_manip::to_buffer< SQLUSMALLINT         >::template from_value< T >::convert(src, dest);
        case SQL_C_LONG:           return value_manip::to_buffer< SQLINTEGER           >::template from_value< T >::convert(src, dest);
        case SQL_C_SLONG:          return value_manip::to_buffer< SQLINTEGER           >::template from_value< T >::convert(src, dest);
        case SQL_C_ULONG:          return value_manip::to_buffer< SQLUINTEGER          >::template from_value< T >::convert(src, dest);
        case SQL_C_SBIGINT:        return value_manip::to_buffer< SQLBIGINT            >::template from_value< T >::convert(src, dest);
        case SQL_C_UBIGINT:        return value_manip::to_buffer< SQLUBIGINT           >::template from_value< T >::convert(src, dest);
        case SQL_C_FLOAT:          return value_manip::to_buffer< SQLREAL              >::template from_value< T >::convert(src, dest);
        case SQL_C_DOUBLE:         return value_manip::to_buffer< SQLDOUBLE            >::template from_value< T >::convert(src, dest);
        case SQL_C_BINARY:         return value_manip::to_buffer< SQLCHAR *            >::template from_value< T >::convert(src, dest, std::forward<ConversionContext>(context));
        case SQL_C_GUID:           return value_manip::to_buffer< SQLGUID              >::template from_value< T >::convert(src, dest);

//      case SQL_C_BOOKMARK:       return value_manip::to_buffer< BOOKMARK             >::template from_value< T >::convert(src, dest);
//      case SQL_C_VARBOOKMARK:    return value_manip::to_buffer< SQLCHAR *            >::template from_value< T >::convert(src, dest);

        case SQL_C_NUMERIC:        return value_manip::to_buffer< SQL_NUMERIC_STRUCT   >::template from_value< T >::convert(src, dest);

        case SQL_C_DATE:
        case SQL_C_TYPE_DATE:      return value_manip::to_buffer< SQL_DATE_STRUCT      >::template from_value< T >::convert(src, dest);

        case SQL_C_TIME:
        case SQL_C_TYPE_TIME:      return value_manip::to_buffer< SQL_TIME_STRUCT      >::template from_value< T >::convert(src, dest);

        case SQL_C_TIMESTAMP:
        case SQL_C_TYPE_TIMESTAMP: return value_manip::to_buffer< SQL_TIMESTAMP_STRUCT >::template from_value< T >::convert(src, dest);

        default:
            throw std::runtime_error("Unable to write data into bound buffer: destination type representation not supported");
    }
}
