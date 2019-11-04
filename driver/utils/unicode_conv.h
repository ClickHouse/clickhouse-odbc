#pragma once

#include "driver/platform/platform.h"

#include <algorithm>
#include <codecvt>
#include <locale>
#include <string>
#include <type_traits>
#include <vector>

inline std::string toUTF8(const char * src, SQLLEN length = SQL_NTS) {
    if (!src || (length != SQL_NTS && length <= 0))
        return {};

    // TODO: convert from the current locale?

    return (length == SQL_NTS ? std::string{src} : std::string{src, static_cast<std::string::size_type>(length)});
}

inline std::string toUTF8(const char16_t * src, SQLLEN length = SQL_NTS) {
    if (!src || (length != SQL_NTS && length <= 0))
        return {};

    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return (length == SQL_NTS ? convert.to_bytes(src) : convert.to_bytes(src, src + length));
}

inline std::string toUTF8(const char32_t * src, SQLLEN length = SQL_NTS) {
    if (!src || (length != SQL_NTS && length <= 0))
        return {};

    std::wstring_convert<std::codecvt_utf8_utf16<char32_t>, char32_t> convert;
    return (length == SQL_NTS ? convert.to_bytes(src) : convert.to_bytes(src, src + length));
}

inline std::string toUTF8(const wchar_t * src, SQLLEN length = SQL_NTS) {
    if (!src || (length != SQL_NTS && length <= 0))
        return {};

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
    return (length == SQL_NTS ? convert.to_bytes(src) : convert.to_bytes(src, src + length));
}

inline auto toUTF8(const signed char * src, SQLLEN length = SQL_NTS) {
    return toUTF8(reinterpret_cast<const char *>(src), length);
}

inline auto toUTF8(const unsigned char * src, SQLLEN length = SQL_NTS) {
    return toUTF8(reinterpret_cast<const char *>(src), length);
}

inline auto toUTF8(const unsigned short * src, SQLLEN length = SQL_NTS) {
    static_assert(sizeof(unsigned short) == sizeof(char16_t));
    return toUTF8(reinterpret_cast<const char16_t *>(src), length);
}

// TODO: use the generic template defined below, when converting from the current locale implemented.
inline const std::string & toUTF8(const std::string & src) {
    return src;
}

template <typename CharType>
inline auto toUTF8(const std::basic_string<CharType> & src) {
    return toUTF8(src.c_str(), src.size());
}

template <typename CharType>
inline auto toUTF8(const std::vector<CharType> & src) {
    return toUTF8((src.empty() ? nullptr : &src[0]), src.size());
}

template <typename CharType>
struct fromUTF8Helper {
    using return_type = std::basic_string<CharType>;
};

template <>
struct fromUTF8Helper<char> {
    // TODO: remove reference, when not the original unchanged string is returned anymore.
    using return_type = const std::string &;
};

template <>
struct fromUTF8Helper<signed char> {
    using return_type = typename fromUTF8Helper<char>::return_type;
};

template <>
struct fromUTF8Helper<unsigned char> {
    using return_type = typename fromUTF8Helper<char>::return_type;
};

template <>
struct fromUTF8Helper<unsigned short> {
    using return_type = typename fromUTF8Helper<char16_t>::return_type;
};

template <typename CharType>
inline typename fromUTF8Helper<CharType>::return_type fromUTF8(const std::string & src); // Leave unimplemented for general case.

template <>
inline typename fromUTF8Helper<char>::return_type fromUTF8<char>(const std::string & src) {
    
    // TODO: convert to the current locale?
    
    return src;
}

template <>
inline typename fromUTF8Helper<char16_t>::return_type fromUTF8<char16_t>(const std::string & src) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.from_bytes(src);
}

template <>
inline typename fromUTF8Helper<char32_t>::return_type fromUTF8<char32_t>(const std::string & src) {
    std::wstring_convert<std::codecvt_utf8_utf16<char32_t>, char32_t> convert;
    return convert.from_bytes(src);
}

template <>
inline typename fromUTF8Helper<wchar_t>::return_type fromUTF8<wchar_t>(const std::string & src) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
    return convert.from_bytes(src);
}

template <>
inline typename fromUTF8Helper<signed char>::return_type fromUTF8<signed char>(const std::string & src) {
    return fromUTF8<char>(src);
}

template <>
inline typename fromUTF8Helper<unsigned char>::return_type fromUTF8<unsigned char>(const std::string & src) {
    return fromUTF8<char>(src);
}

template <>
inline typename fromUTF8Helper<unsigned short>::return_type fromUTF8<unsigned short>(const std::string & src) {
    return fromUTF8<char16_t>(src);
}

template <typename CharType>
inline void fromUTF8(const std::string & src, std::basic_string<CharType> & dest,
    typename std::enable_if<std::is_same<
        std::basic_string<CharType>, typename std::decay<decltype(fromUTF8<CharType>(src))>::type
    >::value>::type * = nullptr
) {
    dest = fromUTF8<CharType>(src);
}

template <typename CharType>
inline void fromUTF8(const std::string & src, std::basic_string<CharType> & dest,
    typename std::enable_if<!std::is_same<
        std::basic_string<CharType>, typename std::decay<decltype(fromUTF8<CharType>(src))>::type
    >::value>::type * = nullptr
) {
    const auto converted = fromUTF8<CharType>(src);
    dest.clear();
    dest.reserve(converted.size() + 1);
    dest.assign(converted.begin(), converted.end());
}

template <typename CharType>
inline void fromUTF8(const std::string & src, std::vector<CharType> & dest) {
    const auto converted = fromUTF8<CharType>(src);
    dest.clear();
    dest.reserve(converted.size() + 1);
    dest.assign(converted.begin(), converted.end());
    dest.emplace_back(0);
}
