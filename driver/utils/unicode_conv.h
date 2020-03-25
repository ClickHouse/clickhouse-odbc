#pragma once

#include "driver/platform/platform.h"

#include <algorithm>
#include <codecvt>
#include <locale>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <cstring>

using CharTypeLPCTSTR = std::remove_cv_t<std::remove_pointer_t<LPCTSTR>>;

template <typename> struct dependent_false : std::false_type {};

template <typename DestCharType, typename SourceCharType>
inline std::basic_string_view<DestCharType> reinterpret_cast_string_view(const std::basic_string_view<SourceCharType> & src) {
    return std::basic_string_view<DestCharType>{reinterpret_cast<const DestCharType *>(src.data()), src.size()};
}

template <typename CharType>
inline std::basic_string_view<CharType> make_string_view(const std::basic_string<CharType> & src) {
    return std::basic_string_view<CharType>{src.c_str(), src.size()};
}


// ConsumeSignature() - return the same string but without provided leading signature/BOM bytes, if any.

template <typename CharType>
inline std::basic_string_view<CharType> ConsumeSignature(std::basic_string_view<CharType> str, const std::string_view & signature) {
    if (
        signature.size() > 0 &&
        (str.size() * sizeof(CharType)) >= signature.size() &&
        std::memcmp(&str[0], &signature[0], signature.size()) == 0
    ) {
        str.remove_prefix(signature.size() / sizeof(CharType));
    }

    return str;
}

template <typename CharType>
inline void ConsumeSignature(std::basic_string<CharType> & str, const std::string_view & signature) {
    if (
        signature.length() > 0 &&
        (str.size() * sizeof(CharType)) >= signature.size() &&
        std::memcmp(&str[0], &signature[0], signature.size()) == 0
    ) {
        str.erase(0, signature.size() / sizeof(CharType));
    }
}


// StringBufferLength() - return the number of elements in the null-terminated buffer (that is assumed to hold a string).

template <typename CharType>
inline std::size_t StringBufferLength(const CharType * str) {
    return (str ? std::basic_string_view<CharType>{str}.size() + 1 : 0);
}


// StringLengthUTF8() - return the number of characters in the UTF-8 string (assuming valid UTF-8).

inline std::size_t StringLengthUTF8(const std::string_view & str) {

/*
    https://tools.ietf.org/html/rfc3629#section-3

    Char. number range  |        UTF-8 octet sequence
       (hexadecimal)    |              (binary)
    --------------------+------------------------------------
    0000 0000-0000 007F | 0xxxxxxx
    0000 0080-0000 07FF | 110xxxxx 10xxxxxx
    0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
    0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
*/

    constexpr unsigned char possible_utf8_signature[] = { 0xEF, 0xBB, 0xBF };
    const std::string_view signature{
        reinterpret_cast<const char *>(possible_utf8_signature),
        sizeof(possible_utf8_signature) / sizeof(possible_utf8_signature[0])
    };

    const auto str_no_sig = ConsumeSignature(str, signature);
    auto len = str_no_sig.size();

    for (std::size_t i = 0; i < str_no_sig.size(); ++i) {
        const auto ch = str_no_sig[i];
        if ((ch & 0b11100000) == 0b11000000) { // ...check for [110xxxxx], then assume [10xxxxxx] follows.
            len -= (len < 1 ? len : 1);
            i += 1;
        }
        else if ((ch & 0b11110000) == 0b11100000) { // ...check for [1110xxxx], then assume [10xxxxxx 10xxxxxx] follow.
            len -= (len < 2 ? len : 2);
            i += 2;
        }
        else if ((ch & 0b11111000) == 0b11110000) { // ...check for [11110xxx], then assume [10xxxxxx 10xxxxxx 10xxxxxx] follow.
            len -= (len < 3 ? len : 3);
            i += 3;
        }
        // else, no need to adjust current length for this character.
    }

    return len;
}

template <typename CharType>
inline std::size_t StringLengthUTF8(const std::basic_string<CharType> & str) {
    return StringLengthUTF8(make_string_view(str));
}

template <typename CharType>
inline std::size_t StringLengthUTF8(const CharType * str, const std::size_t size) {
    if (!str || !size)
        return 0;

    return StringLengthUTF8(std::basic_string_view<CharType>{str, size});
}

template <typename CharType>
inline std::size_t StringLengthUTF8(const CharType * str) {
    return StringLengthUTF8(str, StringBufferLength(str));
}


#if defined(WORKAROUND_USE_ICU)
#   include "driver/utils/unicode_conv_icu.h"
#else
#   include "driver/utils/unicode_conv_std.h"
#endif
