#pragma once

#include <string.h>
#include "log/log.h"
#include "platform.h"
#include "string_ref.h"
#include "unicode_t.h"
#include <iomanip>

template<
    class CharT,
    class Traits = std::char_traits<CharT>,
    class Allocator = std::allocator<CharT>
> 
inline void hex_print(std::ostream &stream, const std::basic_string<CharT, Traits, Allocator>& s)
//inline void hex_print(std::ostream &stream, const std::string& s)
{
    stream << "[" << s.size() << "] " << std::hex << std::setfill('0');
    for(unsigned char c : s)
        stream << std::setw(2) << static_cast<int>(c) << ' ';
    stream << std::dec << '\n';
}

/** Checks `handle`. Catches exceptions and puts them into the DiagnosticRecord.
  */
template <typename Handle, typename F>
RETCODE doWith(SQLHANDLE handle_opaque, F && f) {
    if (nullptr == handle_opaque)
        return SQL_INVALID_HANDLE;

    Handle & handle = *reinterpret_cast<Handle *>(handle_opaque);

    try {
        handle.diagnostic_record.reset();
        return f(handle);
    } catch (...) {
        handle.diagnostic_record.fromException();
        LOG("Exception: " << handle.diagnostic_record.message);
        return SQL_ERROR;
    }
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

hex_print(log_stream, std::string{static_cast<const char *>(static_cast<const void *>(data))});

#if defined(UNICODE)
    //return MY_UTF_W_CONVERT().to_bytes(reinterpret_cast<const MY_STD_T_CHAR *>(data));
// std::wstring_convert<std::codecvt_utf8_utf16<wide_char_t>, wide_char_t>().from_bytes(in);
auto r = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(reinterpret_cast<const char16_t *>(data));
LOG("CONV1.2:" << r);
return r;
const auto tmp = MY_STD_T_STRING(reinterpret_cast<const MY_STD_T_CHAR*>(data), symbols);
LOG("CONV1:" <<  symbols << " : " << tmp.size());
hex_print(log_stream, tmp);
    return MY_UTF_W_CONVERT().to_bytes(tmp);
#else
LOG("CONV2" << symbols);
    return {reinterpret_cast<const char *>(data)};
#endif
}
#endif

template <typename SIZE_TYPE = decltype(SQL_NTS)>
std::string stringFromSQLTSymbols(SQLTCHAR * data, SIZE_TYPE symbols = SQL_NTS) {
    if (!data || symbols == 0 || symbols == SQL_NULL_DATA)
        return {};



/*
    if (symbols == SQL_NTS)
    {
#if defined(UNICODE)
        symbols = wcslen(reinterpret_cast<const wchar_t*>(data));
#else
        symbols = strlen(reinterpret_cast<const char*>(data));
#endif
        // LOG(__FUNCTION__ << " NTS symbols=" << symbols << " sizeof(SQLTCHAR)=" << sizeof(SQLTCHAR) );
    }
    else if (symbols < 0)
        throw std::runtime_error("invalid size of string : " + std::to_string(symbols));
*/


LOG("stringFromSQLSymbols: CONV1.30: [symbols=" << symbols << "]");

if (symbols > 0)
hex_print(log_stream, std::string{static_cast<const char *>(static_cast<const void *>(data)), static_cast<size_t>(symbols)});
//hex_print(log_stream, static_cast<const char *>(static_cast<const void *>(data)));

#if defined(UNICODE)
    return MY_UTF_W_CONVERT().to_bytes(reinterpret_cast<const MY_STD_T_CHAR *>(data));
// std::wstring_convert<std::codecvt_utf8_utf16<wide_char_t>, wide_char_t>().from_bytes(in);

/*std::u16string tmpt{reinterpret_cast<const char16_t *>(data), static_cast<size_t>(symbols)};
auto r = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(tmpt);
//auto r = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(reinterpret_cast<const char16_t *>(data));
LOG("CONV1.3: [symbols=" << symbols << "]" << r);
return r;
*/
const auto tmp = MY_STD_T_STRING(reinterpret_cast<const MY_STD_T_CHAR*>(data), symbols);
LOG("CONV1.31:" <<  symbols << " : " << tmp.size());
hex_print(log_stream, tmp);
auto r = MY_UTF_W_CONVERT().to_bytes(tmp);
LOG("CONV1.3: [symbols=" << symbols << "]" << r);
    return r;

#else
LOG("CONV2" << symbols);
    return {reinterpret_cast<const char *>(data)};
#endif
}

template <typename SIZE_TYPE = decltype(SQL_NTS)>
std::string stringFromSQLSymbols(SQLTCHAR * data, SIZE_TYPE symbols = SQL_NTS) {
#if ODBC_CHAR16
    LOG("stringFromSQLSymbols: use 16" << symbols);
    return stringFromChar16String(data, symbols);
#else
    return stringFromSQLTSymbols(data, symbols);
#endif
}


template <typename SIZE_TYPE = decltype(SQL_NTS)>
std::string stringFromSQLSymbols2(SQLTCHAR * data, SIZE_TYPE symbols = SQL_NTS) {
//#if defined(_IODBCUNIX_H)
#if ODBC_CHAR16
    LOG("stringFromSQLSymbols2: use 16:" << symbols);
    auto ret = stringFromChar16String(data, symbols);
#else
    LOG("stringFromSQLSymbols2: use T:" << symbols);
    auto ret = stringFromSQLSymbols(data, symbols);
#endif
    return ret;
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

LOG("fillOutputStringImpl: " << symbols << " = " /* << value*/ );


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
LOG("fillOutputUSC2String: " << out_value_max_length << " = " /*<< value*/);

/*
#if !ODBC_UNIXODBC && (ODBC_WCHAR || !defined(UNICODE))
    using CharType = wchar_t;
#else
    using CharType = char16_t;
#endif
*/
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

