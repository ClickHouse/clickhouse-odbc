#pragma once

#include "driver/platform/platform.h"

#include <algorithm>
#include <codecvt>
#include <locale>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <cstring>

using CharTypeLPCTSTR = std::remove_cv<std::remove_pointer<LPCTSTR>::type>::type;

inline std::size_t NTSStringLength(const char * src, const std::locale& locale) {

    // TODO: implement and use conversion from the specified locale.

    throw std::runtime_error("not implemented");
}

inline decltype(auto) NTSStringLength(const signed char * src, const std::locale& locale) {
    return NTSStringLength(reinterpret_cast<const char *>(src), locale);
}

inline decltype(auto) NTSStringLength(const unsigned char * src, const std::locale& locale) {
    return NTSStringLength(reinterpret_cast<const char *>(src), locale);
}

inline std::size_t NTSStringLength(const char * src) {
    if (!src)
        return 0;

    // TODO: convert from the current locale by default?

    return std::strlen(src);
}

inline std::size_t NTSStringLength(const char16_t * src) {
    if (!src)
        return 0;

    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
    convert.to_bytes(src);

    return convert.converted();
}

inline std::size_t NTSStringLength(const char32_t * src) {
    if (!src)
        return 0;

    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
    convert.to_bytes(src);

    return convert.converted();
}

inline std::size_t NTSStringLength(const wchar_t * src) {
    if (!src)
        return 0;

    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    convert.to_bytes(src);

    return convert.converted();
}

inline decltype(auto) NTSStringLength(const signed char * src) {
    return NTSStringLength(reinterpret_cast<const char *>(src));
}

inline decltype(auto) NTSStringLength(const unsigned char * src) {
    return NTSStringLength(reinterpret_cast<const char *>(src));
}

inline decltype(auto) NTSStringLength(const unsigned short * src) {
    static_assert(sizeof(unsigned short) == sizeof(char16_t), "unsigned short doesn't match char16_t exactly");
    return NTSStringLength(reinterpret_cast<const char16_t *>(src));
}

inline std::size_t NTSStringLength(const std::string & src, const std::locale& locale) {

    // TODO: implement and use conversion to the specified locale.

    throw std::runtime_error("not implemented");
}

template <typename CharType>
inline std::size_t NTSStringLength(const std::basic_string<CharType> & src) {
    return src.size();
}

inline std::string toUTF8(const char * src, const std::locale& locale, SQLLEN length = SQL_NTS) {

    // TODO: implement and use conversion from the specified locale.

    throw std::runtime_error("not implemented");
}

inline decltype(auto) toUTF8(const signed char * src, const std::locale& locale, SQLLEN length = SQL_NTS) {
    return toUTF8(reinterpret_cast<const char *>(src), locale, length);
}

inline decltype(auto) toUTF8(const unsigned char * src, const std::locale& locale, SQLLEN length = SQL_NTS) {
    return toUTF8(reinterpret_cast<const char *>(src), locale, length);
}

inline std::string toUTF8(const char * src, SQLLEN length = SQL_NTS) {
    if (!src || (length != SQL_NTS && length <= 0))
        return {};

    // TODO: convert from the current locale by default?

    // Workaround for UnixODBC Unicode client vs ANSI driver string encoding issue:
    // strings may be reported with a fixed length that also includes a trailing null character.
    // TODO: review this. This is not a formally right thing to do, but should not cause problems in practice.
#if defined(WORKAROUND_ENABLE_TRIM_TRAILING_NULL)
    if (src && length > 0 && src[length - 1] == '\0')
        --length;
#endif

    return (length == SQL_NTS ? std::string{src} : std::string{src, static_cast<std::string::size_type>(length)});
}

inline std::string toUTF8(const char16_t * src, SQLLEN length = SQL_NTS) {
    if (!src || (length != SQL_NTS && length <= 0))
        return {};

    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
    return (length == SQL_NTS ? convert.to_bytes(src) : convert.to_bytes(src, src + length));
}

inline std::string toUTF8(const char32_t * src, SQLLEN length = SQL_NTS) {
    if (!src || (length != SQL_NTS && length <= 0))
        return {};

    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
    return (length == SQL_NTS ? convert.to_bytes(src) : convert.to_bytes(src, src + length));
}

inline std::string toUTF8(const wchar_t * src, SQLLEN length = SQL_NTS) {
    if (!src || (length != SQL_NTS && length <= 0))
        return {};

    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    return (length == SQL_NTS ? convert.to_bytes(src) : convert.to_bytes(src, src + length));
}

inline decltype(auto) toUTF8(const signed char * src, SQLLEN length = SQL_NTS) {
    return toUTF8(reinterpret_cast<const char *>(src), length);
}

inline decltype(auto) toUTF8(const unsigned char * src, SQLLEN length = SQL_NTS) {
    return toUTF8(reinterpret_cast<const char *>(src), length);
}

inline decltype(auto) toUTF8(const unsigned short * src, SQLLEN length = SQL_NTS) {
    static_assert(sizeof(unsigned short) == sizeof(char16_t), "unsigned short doesn't match char16_t exactly");
    return toUTF8(reinterpret_cast<const char16_t *>(src), length);
}

inline std::string toUTF8(const std::string & src, const std::locale& locale) {

    // TODO: implement and use conversion to the specified locale.

    throw std::runtime_error("not implemented");
}

// Returns cref to the original, to avoid string copy in no-op case, for now.
inline const std::string & toUTF8(const std::string & src) {

    // TODO: convert to the current locale by default?

    return src;
}

template <typename CharType>
inline decltype(auto) toUTF8(const std::basic_string<CharType> & src) {
    return toUTF8(src.c_str(), src.size());
}

template <typename CharType>
inline decltype(auto) fromUTF8(const std::string & src, const std::locale& locale); // Leave unimplemented for general case.

template <>
inline decltype(auto) fromUTF8<char>(const std::string & src, const std::locale& locale) {

    // TODO: implement and use conversion to the specified locale.

    throw std::runtime_error("not implemented");

    return std::string{};
}

template <>
inline decltype(auto) fromUTF8<signed char>(const std::string & src, const std::locale& locale) {
    const auto converted = fromUTF8<char>(src, locale);
    return std::basic_string<signed char>{converted.begin(), converted.end()};
}

template <>
inline decltype(auto) fromUTF8<unsigned char>(const std::string & src, const std::locale& locale) {
    const auto converted = fromUTF8<char>(src, locale);
    return std::basic_string<unsigned char>{converted.begin(), converted.end()};
}

template <typename CharType>
inline decltype(auto) fromUTF8(const std::string & src); // Leave unimplemented for general case.

template <>
inline decltype(auto) fromUTF8<char>(const std::string & src) {

    // TODO: convert to the current locale?

    return src;
}

template <>
inline decltype(auto) fromUTF8<char16_t>(const std::string & src) {
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
    return convert.from_bytes(src);
}

template <>
inline decltype(auto) fromUTF8<char32_t>(const std::string & src) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
    return convert.from_bytes(src);
}

template <>
inline decltype(auto) fromUTF8<wchar_t>(const std::string & src) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    return convert.from_bytes(src);
}

template <>
inline decltype(auto) fromUTF8<signed char>(const std::string & src) {
    const auto converted = fromUTF8<char>(src);
    return std::basic_string<signed char>{converted.begin(), converted.end()};
}

template <>
inline decltype(auto) fromUTF8<unsigned char>(const std::string & src) {
    const auto converted = fromUTF8<char>(src);
    return std::basic_string<unsigned char>{converted.begin(), converted.end()};
}

template <>
inline decltype(auto) fromUTF8<unsigned short>(const std::string & src) {
    static_assert(sizeof(unsigned short) == sizeof(char16_t), "unsigned short doesn't match char16_t exactly");
    const auto converted = fromUTF8<char16_t>(src);
    return std::basic_string<unsigned short>{converted.begin(), converted.end()};
}

template <typename CharType>
inline void fromUTF8(const std::string & src, std::basic_string<CharType> & dest, const std::locale& locale); // Leave unimplemented for general case.

template <>
inline void fromUTF8<char>(const std::string & src, std::string & dest, const std::locale& locale) {
    dest = fromUTF8<char>(src, locale);
}

template <>
inline void fromUTF8<signed char>(const std::string & src, std::basic_string<signed char> & dest, const std::locale& locale) {
    dest = fromUTF8<signed char>(src, locale);
}

template <>
inline void fromUTF8<unsigned char>(const std::string & src, std::basic_string<unsigned char> & dest, const std::locale& locale) {
    dest = fromUTF8<unsigned char>(src, locale);
}

template <typename CharType>
inline void fromUTF8(const std::string & src, std::basic_string<CharType> & dest,
    typename std::enable_if<std::is_assignable<
        std::basic_string<CharType>, decltype(fromUTF8<CharType>(src))
    >::value>::type * = nullptr
) {
    dest = fromUTF8<CharType>(src);
}

template <typename CharType>
inline void fromUTF8(const std::string & src, std::basic_string<CharType> & dest,
    typename std::enable_if<!std::is_assignable<
        std::basic_string<CharType>, decltype(fromUTF8<CharType>(src))
    >::value>::type * = nullptr
) {
    const auto converted = fromUTF8<CharType>(src);
    dest.clear();
    dest.reserve(converted.size() + 1);
    dest.assign(converted.begin(), converted.end());
}
