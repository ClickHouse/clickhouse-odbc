#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/object_pool.h"

#include <string>

class StringPool {
public:
    explicit StringPool(const std::size_t max_size)
        : string_pool_c_   {max_size}
        , string_pool_c16_ {max_size}
        , string_pool_c32_ {max_size}
        , string_pool_wc_  {max_size}
    {
    }

    template <typename CharType>
    inline void retireString(std::basic_string<CharType> && str);

    template <typename CharType>
    inline std::basic_string<CharType> allocateString();

private:
    template <typename CharType>
    inline ObjectPool<std::basic_string<CharType>> & accessStringPool(); // Leave unimplemented for general case.

    ObjectPool< std::basic_string<char>           > string_pool_c_;
    ObjectPool< std::basic_string<char16_t>       > string_pool_c16_;
    ObjectPool< std::basic_string<char32_t>       > string_pool_c32_;
    ObjectPool< std::basic_string<wchar_t>        > string_pool_wc_;
};

template <typename CharType>
inline void StringPool::retireString(std::basic_string<CharType> && str) {
    return accessStringPool<CharType>().put(std::move(str));
}

template <typename CharType>
inline std::basic_string<CharType> StringPool::allocateString() {
    return accessStringPool<CharType>().get();
}

template <>
inline ObjectPool<std::basic_string<char>> & StringPool::accessStringPool<char>() {
    return string_pool_c_;
}

template <>
inline ObjectPool<std::basic_string<char16_t>> & StringPool::accessStringPool<char16_t>() {
    return string_pool_c16_;
}

template <>
inline ObjectPool<std::basic_string<char32_t>> & StringPool::accessStringPool<char32_t>() {
    return string_pool_c32_;
}

template <>
inline ObjectPool<std::basic_string<wchar_t>> & StringPool::accessStringPool<wchar_t>() {
    return string_pool_wc_;
}
