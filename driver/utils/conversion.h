#pragma once

#include "driver/platform/platform.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <cstring>

using CharTypeLPCTSTR = std::remove_cv_t<std::remove_pointer_t<LPCTSTR>>;

template <typename CharType>
inline auto make_string_view(const CharType * src) {
    return std::basic_string_view<CharType>{src};
}

template <typename CharType>
inline auto make_string_view(const CharType * src, const std::size_t size) {
    return std::basic_string_view<CharType>{src, size};
}

template <typename CharType>
inline auto make_string_view(const std::basic_string<CharType> & src) {
    return std::basic_string_view<CharType>{src.c_str(), src.size()};
}


// stringBufferLength() - return the number of elements in the null-terminated buffer (that is assumed to hold a string).

template <typename CharType>
inline std::size_t stringBufferLength(const CharType * str) {
    return (str ? std::basic_string_view<CharType>{str}.size() + 1 : 0);
}

template <typename CharType>
inline std::size_t stringLengthUTF8(const std::basic_string<CharType> & str) {
    return stringLengthUTF8(make_string_view(str));
}

template <typename CharType>
inline std::size_t stringLengthUTF8(const CharType * str, const std::size_t size) {
    if (!str || !size)
        return 0;

    return stringLengthUTF8(std::basic_string_view<CharType>{str, size});
}

template <typename CharType>
inline std::size_t stringLengthUTF8(const CharType * str) {
    return stringLengthUTF8(str, stringBufferLength(str));
}


#if defined(WORKAROUND_USE_ICU)
#   include "driver/utils/conversion_icu.h"
#else
#   include "driver/utils/conversion_std.h"
#endif
