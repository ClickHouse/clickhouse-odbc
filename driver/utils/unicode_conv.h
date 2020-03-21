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


// ConsumeBOMUTF8() - return the same string but without leading BOM bytes, if any (assuming UTF-8 string).

template <typename CharType>
inline std::basic_string_view<CharType> ConsumeBOMUTF8(const std::basic_string_view<CharType> str) {
    auto res = str;

    if (
        res.size() > 2 &&
        res[0] == 0xEF &&
        res[1] == 0xBB &&
        res[2] == 0xBF
    ) {
        res.remove_prefix(3);
    }

    return res;
}


// ConsumeBOMUTF16() - return the same string but without leading BOM bytes, if any (assuming UTF-16 string).

template <typename CharType>
inline std::basic_string_view<CharType> ConsumeBOMUTF16(const std::basic_string_view<CharType> str) {
    auto res = str;

    if (
        res.size() > 0 &&
        res[0] == 0xFEFF
    ) {
        res.remove_prefix(1);
    }

    return res;
}


// StringBufferLength() - return the number of elements in the null-terminated buffer (that is assumed to hold a string).

template <typename CharType>
inline std::size_t StringBufferLength(const CharType * str) {
    return (str ? std::basic_string_view<CharType>{str}.size() + 1 : 0);
}


// StringLengthUTF8() - return the number of characters in the UTF-8 string (assuming valid UTF-8).

template <typename CharType>
inline std::size_t StringLengthUTF8(const std::basic_string_view<CharType> str) {

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

    const auto str_no_bom = ConsumeBOMUTF8(str);
    auto len = str_no_bom.size();

    for (std::size_t i = 0; i < str_no_bom.size(); ++i) {
        const auto ch = str_no_bom[i];
        if ((ch && 0b11100000) == 0b11000000) { // ...check for [110xxxxx], then assume [10xxxxxx] follows.
            len -= (len < 1 ? len : 1);
            i += 1;
        }
        else if ((ch && 0b11110000) == 0b11100000) { // ...check for [1110xxxx], then assume [10xxxxxx 10xxxxxx] follow.
            len -= (len < 2 ? len : 2);
            i += 2;
        }
        else if ((ch && 0b11111000) == 0b11110000) { // ...check for [11110xxx], then assume [10xxxxxx 10xxxxxx 10xxxxxx] follow.
            len -= (len < 3 ? len : 3);
            i += 3;
        }
        // else, no need to adjust current length for this character.
    }

    return len;
}

template <typename CharType>
inline std::size_t StringLengthUTF8(const std::basic_string<CharType> str) {
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
