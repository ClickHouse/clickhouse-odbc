#pragma once

#include "platform.h"
#include "string_ref.h"
#include "unicode_t.h"

#ifndef NDEBUG
#    if USE_DEBUG_17
#        include "iostream_debug_helpers.h"
#    endif
#endif

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <type_traits>

#include <cstring>

#ifdef _win_
#   include <processthreadsapi.h>
#else
#   include <sys/types.h>
#   include <unistd.h>
#endif

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

template <typename T> constexpr auto & get_object_type_name() { return "HANDLE"; }
template <> constexpr auto & get_object_type_name<Environment>() { return "ENV"; }
template <> constexpr auto & get_object_type_name<Connection>() { return "DBC"; }
template <> constexpr auto & get_object_type_name<Descriptor>() { return "DESC"; }
template <> constexpr auto & get_object_type_name<Statement>() { return "STMT"; }

template <typename T> constexpr int get_object_handle_type() { return 0; }
template <> constexpr int get_object_handle_type<Environment>() { return SQL_HANDLE_ENV; }
template <> constexpr int get_object_handle_type<Connection>() { return SQL_HANDLE_DBC; }
template <> constexpr int get_object_handle_type<Descriptor>() { return SQL_HANDLE_DESC; }
template <> constexpr int get_object_handle_type<Statement>() { return SQL_HANDLE_STMT; }

template <typename T>
inline std::string to_hex_string(T n) {
    std::stringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << n;
    return stream.str();
}

inline auto get_pid() {
#ifdef _win_
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

inline auto get_tid() {
    return std::this_thread::get_id();
}

inline std::string to_upper_copy(const std::string & str) {
    std::string ret{str};
    std::transform(ret.begin(), ret.end(), ret.begin(), ::toupper);
    return ret;
}

template<
    class CharT,
    class Traits = std::char_traits<CharT>,
    class Allocator = std::allocator<CharT>
>
inline void hex_print(std::ostream &stream, const std::basic_string<CharT, Traits, Allocator>& s)
{
    stream << "[" << s.size() << "] " << std::hex << std::setfill('0');
    for(unsigned char c : s)
        stream << std::setw(2) << static_cast<int>(c) << ' ';
    stream << std::dec << '\n';
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

#if ODBC_CHAR16
template <typename SIZE_TYPE = decltype(SQL_NTS)>
std::string stringFromChar16String(SQLTCHAR * data, SIZE_TYPE symbols = SQL_NTS) {
    if (!data || symbols == 0 || symbols == SQL_NULL_DATA)
        return {};

#if defined(UNICODE)
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(reinterpret_cast<const char16_t *>(data));
#else
    return {reinterpret_cast<const char *>(data)};
#endif
}
#endif

template <typename SIZE_TYPE = decltype(SQL_NTS)>
std::string stringFromSQLTSymbols(SQLTCHAR * data, SIZE_TYPE symbols = SQL_NTS) {
    if (!data || symbols == 0 || symbols == SQL_NULL_DATA)
        return {};

#if defined(UNICODE)
    return MY_UTF_W_CONVERT().to_bytes(reinterpret_cast<const MY_STD_T_CHAR *>(data));
#else
    return {reinterpret_cast<const char *>(data)};
#endif
}

template <typename SIZE_TYPE = decltype(SQL_NTS)>
std::string stringFromSQLSymbols(SQLTCHAR * data, SIZE_TYPE symbols = SQL_NTS) {
#if ODBC_CHAR16
    return stringFromChar16String(data, symbols);
#else
    return stringFromSQLTSymbols(data, symbols);
#endif
}


template <typename SIZE_TYPE = decltype(SQL_NTS)>
std::string stringFromSQLSymbols2(SQLTCHAR * data, SIZE_TYPE symbols = SQL_NTS) {
#if ODBC_CHAR16
    return stringFromChar16String(data, symbols);
#else
    return stringFromSQLSymbols(data, symbols);
#endif
}

template <typename SIZE_TYPE = decltype(SQL_NTS)>
std::string stringFromSQLBytes(SQLTCHAR * data, SIZE_TYPE size = SQL_NTS) {
    if (!data || size == 0)
        return {};
    // Count of symblols in the string
    size_t symbols = 0;
    if (size == SQL_IS_POINTER || size == SQL_IS_UINTEGER || size == SQL_IS_INTEGER || size == SQL_IS_USMALLINT
        || size == SQL_IS_SMALLINT) {
        throw std::runtime_error("SQL data is not a string");
    } else if (size < 0) {
        return {reinterpret_cast<const char *>(data), (size_t)SQL_LEN_BINARY_ATTR(size)};
    } else {
        symbols = static_cast<size_t>(size) / sizeof(SQLTCHAR);
    }

    return stringFromSQLSymbols(data, symbols);
}

inline std::string stringFromMYTCHAR(MYTCHAR * data) {
    return stringFromSQLTSymbols(reinterpret_cast<SQLTCHAR *>(data));
}

/*
inline std::string stringFromTCHAR(SQLTCHAR * data) {
    return stringFromSQLSymbols(data);
}
*/

template <size_t Len, typename STRING>
void stringToTCHAR(const std::string & data, STRING (&result)[Len]) {
#if defined(UNICODE)
    using CharType = MY_STD_T_CHAR;
    using StringType = MY_STD_T_STRING;

    StringType tmp = MY_UTF_W_CONVERT().from_bytes(data);
#else
    const auto & tmp = data;
#endif

    const size_t len = std::min<size_t>(Len - 1, data.size());

#if defined(UNICODE)
#    if ODBC_WCHAR
    wcsncpy(reinterpret_cast<CharType *>(result), tmp.c_str(), len);
#    else
    memcpy(reinterpret_cast<char *>(result), reinterpret_cast<const char *>(tmp.c_str()), len * sizeof(CharType));
#    endif
#else
    strncpy(reinterpret_cast<char *>(result), tmp.c_str(), len);
#endif
    result[len] = 0;
}

template <typename STRING, typename PTR, typename LENGTH>
RETCODE fillOutputStringImpl(
    const STRING & value, PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length, bool length_in_bytes) {
    using CharType = typename STRING::value_type;
    LENGTH symbols = static_cast<LENGTH>(value.size());

    if (out_value_length) {
        if (length_in_bytes)
            *out_value_length = symbols * sizeof(CharType);
        else
            *out_value_length = symbols;
    }

    if (out_value_max_length < 0)
        return SQL_ERROR;

    if (out_value) {
        size_t max_length_in_bytes;

        if (length_in_bytes)
            max_length_in_bytes = out_value_max_length;
        else
            max_length_in_bytes = out_value_max_length * sizeof(CharType);

        if (max_length_in_bytes >= (symbols + 1) * sizeof(CharType)) {
            memcpy(out_value, value.c_str(), (symbols + 1) * sizeof(CharType));
        } else {
            if (max_length_in_bytes >= sizeof(CharType)) {
                memcpy(out_value, value.data(), max_length_in_bytes - sizeof(CharType));
                reinterpret_cast<CharType *>(out_value)[(max_length_in_bytes / sizeof(CharType)) - 1] = 0;
            }
            return SQL_SUCCESS_WITH_INFO;
        }
    }

    return SQL_SUCCESS;
}

template <typename PTR, typename LENGTH>
RETCODE fillOutputRawString(const std::string & value, PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length) {
    return fillOutputStringImpl(value, out_value, out_value_max_length, out_value_length, true);
}

template <typename PTR, typename LENGTH>
RETCODE fillOutputUSC2String(
    const std::string & value, PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length, bool length_in_bytes = true) {
    using CharType = MY_STD_W_CHAR;

    return fillOutputStringImpl(
#if ODBC_CHAR16
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().from_bytes(value),
#else
        std::wstring_convert<std::codecvt_utf8<CharType>, CharType>().from_bytes(value),
#endif
        out_value,
        out_value_max_length,
        out_value_length,
        length_in_bytes);
}

template <typename PTR, typename LENGTH>
RETCODE fillOutputPlatformString(
    const std::string & value, PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length, bool length_in_bytes = true) {
#if defined(UNICODE)
    return fillOutputUSC2String(value, out_value, out_value_max_length, out_value_length, length_in_bytes);
#else
    return fillOutputRawString(value, out_value, out_value_max_length, out_value_length);
#endif
}


template <typename NUM, typename PTR, typename LENGTH>
RETCODE fillOutputNumber(NUM num, PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length) {
    if (out_value_length)
        *out_value_length = sizeof(num);

    if (out_value_max_length < 0)
        return SQL_ERROR;

    bool res = SQL_SUCCESS;

    if (out_value) {
        if (out_value_max_length == 0 || out_value_max_length >= static_cast<LENGTH>(sizeof(num))) {
            memcpy(out_value, &num, sizeof(num));
        } else {
            memcpy(out_value, &num, out_value_max_length);
            res = SQL_SUCCESS_WITH_INFO;
        }
    }

    return res;
}

inline RETCODE fillOutputNULL(PTR out_value, SQLLEN out_value_max_length, SQLLEN * out_value_length) {
    if (out_value_length)
        *out_value_length = SQL_NULL_DATA;
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
        return fillOutputNumber<TYPE>(VALUE, out_value, out_value_max_length, out_value_length);

#if defined(UNICODE)
#    define FUNCTION_MAYBE_W(NAME) NAME##W
#else
#    define FUNCTION_MAYBE_W(NAME) NAME
#endif

