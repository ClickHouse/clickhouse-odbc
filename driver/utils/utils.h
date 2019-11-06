#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/unicode_conv.h"
#include "driver/utils/string_ref.h"
#include "driver/type_info.h"
#include "driver/exception.h"

#ifndef NDEBUG
#    if USE_DEBUG_17
#        include "driver/utils/iostream_debug_helpers.h"
#    endif
#endif

#include <Poco/NumberParser.h>
#include <Poco/String.h>

#ifdef _win_
#   include <processthreadsapi.h>
#else
#   include <sys/types.h>
#   include <unistd.h>
#endif

#include <algorithm>
#include <functional>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <type_traits>
#include <vector>

#include <cstring>

#if __cplusplus >= 201703L

using std::is_invocable;
using std::is_invocable_r;

#else

template <typename F, typename... Args>
struct is_invocable :
    std::is_constructible<
        std::function<void(Args...)>,
        std::reference_wrapper<typename std::remove_reference<F>::type>
    >
{
};

template <typename R, typename F, typename... Args>
struct is_invocable_r
    : public std::is_constructible<
        std::function<R(Args...)>,
        std::reference_wrapper<typename std::remove_reference<F>::type>
    >
{
};

#endif

class Environment;
class Connection;
class Descriptor;
class Statement;

template <typename T> constexpr auto & getObjectTypeName() { return "HANDLE"; }
template <> constexpr auto & getObjectTypeName<Environment>() { return "ENV"; }
template <> constexpr auto & getObjectTypeName<Connection>() { return "DBC"; }
template <> constexpr auto & getObjectTypeName<Descriptor>() { return "DESC"; }
template <> constexpr auto & getObjectTypeName<Statement>() { return "STMT"; }

template <typename T> constexpr int getObjectHandleType() { return 0; }
template <> constexpr int getObjectHandleType<Environment>() { return SQL_HANDLE_ENV; }
template <> constexpr int getObjectHandleType<Connection>() { return SQL_HANDLE_DBC; }
template <> constexpr int getObjectHandleType<Descriptor>() { return SQL_HANDLE_DESC; }
template <> constexpr int getObjectHandleType<Statement>() { return SQL_HANDLE_STMT; }

template <typename T>
inline constexpr T * ptr_rm_const(const T * ptr) noexcept {
    return const_cast<T *>(ptr);
}

template <typename T>
inline constexpr void * vptr_rm_const(const T * ptr) noexcept {
    return ptr_rm_const(ptr);
}

template <typename T>
inline std::string toHexString(T n) {
    std::stringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << n;
    return stream.str();
}

inline auto getPID() {
#ifdef _win_
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

inline auto getTID() {
    return std::this_thread::get_id();
}

template<
    class CharT,
    class Traits = std::char_traits<CharT>,
    class Allocator = std::allocator<CharT>
>
inline void hexPrint(std::ostream &stream, const std::basic_string<CharT, Traits, Allocator>& s)
{
    stream << "[" << s.size() << "] " << std::hex << std::setfill('0');
    for(unsigned char c : s)
        stream << std::setw(2) << static_cast<int>(c) << ' ';
    stream << std::dec << '\n';
}

inline bool isYes(std::string str) {
    Poco::trimInPlace(str);
    Poco::toLowerInPlace(str);
    bool flag = false;
    return (Poco::NumberParser::tryParseBool(str, flag) ? flag : false);
}

inline auto tryStripParamPrefix(std::string param_name) {
    if (!param_name.empty() && param_name[0] == '@')
        param_name.erase(0, 1);
    return param_name;
}

/// Parse a string of the form `key1=value1;key2=value2` ... TODO Parsing values in curly brackets.
static const char * nextKeyValuePair(const char * data, const char * end, StringRef & out_key, StringRef & out_value) {
    if (data >= end)
        return nullptr;

    const char * key_begin = data;
    const char * key_end = reinterpret_cast<const char *>(memchr(key_begin, '=', end - key_begin));
    if (!key_end)
        return nullptr;

    const char * value_begin = key_end + 1;
    const char * value_end;
    if (value_begin >= end)
        value_end = value_begin;
    else {
        value_end = reinterpret_cast<const char *>(memchr(value_begin, ';', end - value_begin));
        if (!value_end)
            value_end = end;
    }

    out_key.data = key_begin;
    out_key.size = key_end - key_begin;

    out_value.data = value_begin;
    out_value.size = value_end - value_begin;

    if (value_end < end && *value_end == ';')
        return value_end + 1;
    return value_end;
}

// Directly write raw bytes to the buffer.
template <typename LengthType1, typename LengthType2, typename LengthType3>
inline SQLRETURN fillOutputBuffer(
    const void * in_value,
    LengthType1 in_value_length,
    void * out_value,
    LengthType2 out_value_max_length,
    LengthType3 * out_value_length
) {
    if (in_value_length < 0 || (in_value_length > 0 && !in_value))
        return SQL_ERROR;

    if (out_value_length)
        *out_value_length = in_value_length;

    if (!out_value || in_value_length == 0)
        return SQL_SUCCESS;

    if (out_value_max_length < 0)
        throw SqlException("Invalid string or buffer length", "HY090");

    if (out_value_max_length == 0)
        out_value_max_length = in_value_length;

    const auto bytes_to_copy = std::min<std::int64_t>(in_value_length, out_value_max_length);
    std::memcpy(out_value, in_value, bytes_to_copy);

    return (bytes_to_copy > out_value_max_length ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS);
}

// Change encoding, when appropriate, and write the result to the buffer.
// Extra string copy happens here for wide char strings, and strings that require encoding change.
template <typename CharType, typename LengthType1, typename LengthType2>
inline SQLRETURN fillOutputString(
    const std::string & in_value,
    void * out_value,
    LengthType1 out_value_max_length,
    LengthType2 * out_value_length,
    bool in_length_in_bytes,
    bool out_length_in_bytes,
    bool ensure_nts
) {
    if (in_length_in_bytes && out_value && (out_value_max_length % sizeof(CharType)) != 0)
        throw SqlException("Invalid string or buffer length", "HY090");

    if (out_value && out_value_max_length <= 0)
        throw SqlException("Invalid string or buffer length", "HY090");

    decltype(auto) converted = fromUTF8<CharType>(in_value);

    const auto converted_length_in_symbols = converted.size();
    const auto converted_length_in_bytes = converted_length_in_symbols * sizeof(CharType);
    const auto out_value_max_length_in_symbols = (in_length_in_bytes ? out_value_max_length / sizeof(CharType) : out_value_max_length);
    const auto out_value_max_length_in_bytes = (in_length_in_bytes ? out_value_max_length : out_value_max_length * sizeof(CharType));

    auto rc = fillOutputBuffer(
        converted.data(),
        converted_length_in_bytes,
        out_value,
        out_value_max_length_in_bytes,
        out_value_length
    );

    if (!out_length_in_bytes && out_value_length)
        *out_value_length /= sizeof(CharType);

    if (ensure_nts && SQL_SUCCEEDED(rc)) {
        if ((converted_length_in_symbols + 1) >= out_value_max_length_in_symbols) {
            if (out_value)
                reinterpret_cast<CharType *>(out_value)[out_value_max_length_in_symbols] = 0;
            rc = SQL_SUCCESS_WITH_INFO;
        }
        else {
            if (out_value)
                reinterpret_cast<CharType *>(out_value)[converted_length_in_symbols] = 0;
        }
    }

    return rc;
}

template <typename CharType, typename LengthType1, typename LengthType2>
inline SQLRETURN fillOutputString(
    const std::string & in_value,
    void * out_value,
    LengthType1 out_value_max_length,
    LengthType2 * out_value_length,
    bool length_in_bytes
) {
    return fillOutputString<CharType>(
        in_value,
        out_value,
        out_value_max_length,
        out_value_length,
        length_in_bytes,
        length_in_bytes,
        true
    );
}

// ObjectType, that is a pointer type, is treated as an integer, the value of that pointer.
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
    if (out_value_length)
        *out_value_length = SQL_NULL_DATA;
    else
        throw SqlException("Indicator variable required but not supplied", "22002");

    return SQL_SUCCESS;
}


/// See for example info.cpp

#define CASE_FALLTHROUGH(NAME) \
    case NAME:                 \
        if (!name)             \
            name = #NAME;

#define CASE_NUM(NAME, TYPE, VALUE)                                                                  \
    case NAME:                                                                                       \
        if (!name)                                                                                   \
            name = #NAME;                                                                            \
        LOG("GetInfo " << name << ", type: " << #TYPE << ", value: " << #VALUE << " = " << (VALUE)); \
        return fillOutputPOD<TYPE>(VALUE, out_value,                                                 \
            std::decay<decltype(*out_value_length)>::type{0}/* out_value_max_length */, out_value_length);
