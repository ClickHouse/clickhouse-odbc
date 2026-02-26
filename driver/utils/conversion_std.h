#pragma once

#include "driver/utils/string_pool.h"

#include <codecvt>
#include <locale>
#include <string>
#include <type_traits>

class UnicodeConversionContext {
public:
    StringPool string_pool{10};

    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> utf8_utf16_converter;
};

// In future, this will become an aggregate context that will do proper date/time, etc., conversions also.
using DefaultConversionContext = UnicodeConversionContext;

inline std::string toUTF8(const char * src, const std::locale & locale, SQLLEN length = SQL_NTS) {

    // TODO: implement and use conversion from the specified locale.

    throw std::runtime_error("not implemented");
}

inline decltype(auto) toUTF8(const signed char * src, const std::locale & locale, SQLLEN length = SQL_NTS) {
    return toUTF8(reinterpret_cast<const char *>(src), locale, length);
}

inline decltype(auto) toUTF8(const unsigned char * src, const std::locale & locale, SQLLEN length = SQL_NTS) {
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

    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return (length == SQL_NTS ? convert.to_bytes(src) : convert.to_bytes(src, src + length));
}

inline std::string toUTF8(const wchar_t * src, SQLLEN length = SQL_NTS) {
    if (!src || (length != SQL_NTS && length <= 0))
        return {};

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
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

inline std::string toUTF8(const std::string & src, const std::locale & locale) {

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
inline decltype(auto) fromUTF8(const std::string & src, UnicodeConversionContext & context); // Leave unimplemented for general case.

template <>
inline decltype(auto) fromUTF8<char>(const std::string & src, UnicodeConversionContext & context) {

    // TODO: implement conversion between specified locales.

    return src;
}

template <>
inline decltype(auto) fromUTF8<signed char>(const std::string & src, UnicodeConversionContext & context) {
    auto && converted = fromUTF8<char>(src, context);
    return std::basic_string<signed char>{converted.begin(), converted.end()};
}

template <>
inline decltype(auto) fromUTF8<unsigned char>(const std::string & src, UnicodeConversionContext & context) {
    auto && converted = fromUTF8<char>(src, context);
    return std::basic_string<unsigned char>{converted.begin(), converted.end()};
}

#if !defined(_MSC_VER) || _MSC_VER >= 1920
template <>
inline decltype(auto) fromUTF8<char16_t>(const std::string & src, UnicodeConversionContext & context) {
    return context.utf8_utf16_converter.from_bytes(src);
}

#endif

#if !defined(_MSC_VER) || _MSC_VER >= 1920
template <>
inline decltype(auto) fromUTF8<unsigned short>(const std::string & src, UnicodeConversionContext & context) {
    static_assert(sizeof(unsigned short) == sizeof(char16_t), "unsigned short doesn't match char16_t exactly");
    auto && converted = fromUTF8<char16_t>(src, context);
    return std::basic_string<unsigned short>{converted.begin(), converted.end()};
}
#endif

template <typename CharType>
inline decltype(auto) fromUTF8(const std::string & src) {
    UnicodeConversionContext context;
    return fromUTF8<CharType>(src, context);
}

template <typename CharType>
inline void fromUTF8(const std::string & src, std::basic_string<CharType> & dest, UnicodeConversionContext & context,
    typename std::enable_if_t<
        std::is_assignable_v<std::basic_string<CharType>, decltype(fromUTF8<CharType>(src))>
    > * = nullptr
) {
    dest = fromUTF8<CharType>(src, context);
}

template <typename CharType>
inline void fromUTF8(const std::string & src, std::basic_string<CharType> & dest, UnicodeConversionContext & context,
    typename std::enable_if_t<
        !std::is_assignable_v<std::basic_string<CharType>, decltype(fromUTF8<CharType>(src))> &&
        std::is_assignable_v<CharType, typename decltype(fromUTF8<CharType>(src))::char_type>
    > * = nullptr
) {
    auto && converted = fromUTF8<CharType>(src, context);
    dest.clear();
    dest.reserve(converted.size() + 1);
    dest.assign(converted.begin(), converted.end());
}

template <typename CharType>
inline decltype(auto) fromUTF8(const std::string & src, std::basic_string<CharType> & dest) {
    UnicodeConversionContext context;
    return fromUTF8<CharType>(src, dest, context);
}
