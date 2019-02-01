#pragma once

#include "log/log.h"
#include "string_ref.h"
#include "platform.h"

#include <codecvt>
#include <locale>
#include <string.h>

/** Checks `handle`. Catches exceptions and puts them into the DiagnosticRecord.
  */
template <typename Handle, typename F>
RETCODE doWith(SQLHANDLE handle_opaque, F && f)
{
    if (nullptr == handle_opaque)
        return SQL_INVALID_HANDLE;

    Handle & handle = *reinterpret_cast<Handle *>(handle_opaque);

    try
    {
        handle.diagnostic_record.reset();
        return f(handle);
    }
    catch (...)
    {
        handle.diagnostic_record.fromException();
        LOG("Exception: " << handle.diagnostic_record.message);
        return SQL_ERROR;
    }
}


/// Parse a string of the form `key1=value1;key2=value2` ... TODO Parsing values in curly brackets.
static const char * nextKeyValuePair(const char * data, const char * end, StringRef & out_key, StringRef & out_value)
{
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
    else
    {
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

template <typename SIZE_TYPE = decltype(SQL_NTS)>
std::string stringFromSQLSymbols(SQLTCHAR * data, SIZE_TYPE symbols = SQL_NTS)
{
    if (!data || symbols == 0 || symbols == SQL_NULL_DATA)
        return{};
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

#if defined(UNICODE)
#   if ODBC_WCHAR
    return std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(std::wstring(data, symbols));
#   else
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(std::u16string(reinterpret_cast<const char16_t*>(data), symbols));
#   endif
#else
    return{ (const char*)data, (size_t)symbols };
#endif
}

template <typename SIZE_TYPE = decltype(SQL_NTS)>
std::string stringFromSQLBytes(SQLTCHAR * data, SIZE_TYPE size = SQL_NTS)
{
    if (!data || size == 0)
        return {};
    // Count of symblols in the string
    size_t symbols = 0;
    if (size == SQL_IS_POINTER || size == SQL_IS_UINTEGER ||
             size == SQL_IS_INTEGER || size == SQL_IS_USMALLINT ||
             size == SQL_IS_SMALLINT)
    {
        throw std::runtime_error("SQL data is not a string");
    }
    else if (size < 0)
    {
        return{ reinterpret_cast<const char*>(data),  (size_t)SQL_LEN_BINARY_ATTR(size) };
    }
    else
    {
        symbols = static_cast<size_t>(size) / sizeof(SQLTCHAR);
    }

    return stringFromSQLSymbols(data, symbols);
}

inline std::string stringFromMYTCHAR(MYTCHAR * data)
{
    return stringFromSQLSymbols(reinterpret_cast<SQLTCHAR*>(data));
}

inline std::string stringFromTCHAR(SQLTCHAR * data)
{
    return stringFromSQLSymbols(data);
}

template <size_t Len, typename STRING>
void stringToTCHAR(const std::string & data, STRING (&result)[Len])
{
#if defined(UNICODE)
#   if ODBC_WCHAR
    using CharType = wchar_t;
    using StringType = std::wstring;
#   else
    using CharType = char16_t;
    using StringType = std::u16string;
#   endif

    StringType tmp = std::wstring_convert<std::codecvt_utf8<CharType>, CharType>().from_bytes(data);
    //using type_to = wchar_t*;
#else
    const auto & tmp = data;
    //using type_to = char*;
#endif
    const size_t len = std::min<size_t>(Len - 1, data.size());
#if defined(UNICODE)
#   if ODBC_WCHAR
    wcsncpy(reinterpret_cast<CharType*>(result), tmp.c_str(), len);
#else
    strncpy(reinterpret_cast<char*>(result), reinterpret_cast<const char*>(tmp.c_str()), len * sizeof(CharType)); // WRONG TODO
#endif
#else
    strncpy(reinterpret_cast<char*>(result), tmp.c_str(), len);
#endif
    result[len] = 0;
}

template <typename STRING, typename PTR, typename LENGTH>
RETCODE fillOutputStringImpl(const STRING & value,
                             PTR out_value,
                             LENGTH out_value_max_length,
                             LENGTH * out_value_length,
                             bool length_in_bytes)
{
    using CharType = typename STRING::value_type;
    LENGTH symbols = static_cast<LENGTH>(value.size());

    if (out_value_length)
    {
        if (length_in_bytes)
            *out_value_length = symbols * sizeof(CharType);
        else
            *out_value_length = symbols;
    }

    if (out_value_max_length < 0)
        return SQL_ERROR;

    if (out_value)
    {
        size_t max_length_in_bytes;

        if (length_in_bytes)
            max_length_in_bytes = out_value_max_length;
        else
            max_length_in_bytes = out_value_max_length * sizeof(CharType);

        if (max_length_in_bytes >= (symbols + 1) * sizeof(CharType))
        {
            memcpy(out_value, value.c_str(), (symbols + 1) * sizeof(CharType));
        }
        else
        {
            if (max_length_in_bytes >= sizeof(CharType))
            {
                memcpy(out_value, value.data(), max_length_in_bytes - sizeof(CharType));
                reinterpret_cast<CharType*>(out_value)[(max_length_in_bytes / sizeof(CharType)) - 1] = 0;
            }
            return SQL_SUCCESS_WITH_INFO;
        }
    }

    return SQL_SUCCESS;
}

template <typename PTR, typename LENGTH>
RETCODE fillOutputRawString(const std::string & value,
    PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length)
{
    return fillOutputStringImpl(value, out_value, out_value_max_length, out_value_length, true);
}

template <typename PTR, typename LENGTH>
RETCODE fillOutputUSC2String(const std::string & value,
    PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length, bool length_in_bytes = true)
{
#if ODBC_WCHAR || !defined(UNICODE)
    using CharType = wchar_t;
#else
    using CharType = char16_t;
#endif
    return fillOutputStringImpl(
        std::wstring_convert<std::codecvt_utf8<CharType>, CharType>().from_bytes(value),
        out_value, out_value_max_length, out_value_length, length_in_bytes);
}

template <typename PTR, typename LENGTH>
RETCODE fillOutputPlatformString(
    const std::string & value,
    PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length, bool length_in_bytes = true)
{
#if defined(UNICODE)
    return fillOutputUSC2String(value, out_value, out_value_max_length, out_value_length, length_in_bytes);
#else
    return fillOutputRawString(value, out_value, out_value_max_length, out_value_length);
#endif
}


template <typename NUM, typename PTR, typename LENGTH>
RETCODE fillOutputNumber(NUM num,
    PTR out_value, LENGTH out_value_max_length, LENGTH * out_value_length)
{
    if (out_value_length)
        *out_value_length = sizeof(num);

    if (out_value_max_length < 0)
        return SQL_ERROR;

    bool res = SQL_SUCCESS;

    if (out_value)
    {
        if (out_value_max_length == 0 || out_value_max_length >= static_cast<LENGTH>(sizeof(num)))
        {
            memcpy(out_value, &num, sizeof(num));
        }
        else
        {
            memcpy(out_value, &num, out_value_max_length);
            res = SQL_SUCCESS_WITH_INFO;
        }
    }

    return res;
}

inline RETCODE fillOutputNULL(PTR out_value, SQLLEN out_value_max_length, SQLLEN * out_value_length)
{
    if (out_value_length)
        *out_value_length = SQL_NULL_DATA;
    return SQL_SUCCESS;
}


/// See for example info.cpp

#define CASE_FALLTHROUGH(NAME) \
    case NAME: \
        if (!name) name = #NAME;

#define CASE_NUM(NAME, TYPE, VALUE) \
    case NAME: \
        if (!name) name = #NAME; \
        LOG("GetInfo " << name << ", type: " << #TYPE << ", value: " << #VALUE << " = " << (VALUE)); \
        return fillOutputNumber<TYPE>(VALUE, out_value, out_value_max_length, out_value_length);

#if defined(UNICODE)
#   define FUNCTION_MAYBE_W(NAME) NAME ## W
#else
#   define FUNCTION_MAYBE_W(NAME) NAME
#endif
