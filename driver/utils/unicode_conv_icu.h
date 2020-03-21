#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/object_pool.h"
#include "driver/utils/resize_without_initialization.h"

#include <unicode/ustring.h>
#include <unicode/ucnv.h>

#include <algorithm>
#include <codecvt>
#include <locale>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <cstring>

using CharTypeLPCTSTR = std::remove_cv_t<std::remove_pointer_t<LPCTSTR>>;
using ConverterPivotCharType = UChar;
using DriverPivotCharType = char;

template <typename> struct dependent_false : std::false_type {};

class UnicodeConversionContext {
public:
    inline static const std::string converter_pivot_wide_char_encoding = "UTF-16";
    inline static const std::string driver_pivot_narrow_char_encoding = "UTF-8";

private:
    static UConverter * createConverter(const std::string & ecnoding) {
        UErrorCode error_code = U_ZERO_ERROR;
        UConverter * converter = ucnv_open(ecnoding.c_str(), &error_code);

        if (U_FAILURE(error_code))
            throw std::runtime_error(u_errorName(error_code));

        if (!converter)
            throw std::runtime_error("ucnv_open(" + ecnoding + ") failed");

        return converter;
    }

    static void destroyConverter(UConverter * & converter) noexcept {
        if (converter) {
            ucnv_close(converter);
            converter = nullptr;
        }
    }

    static bool sameEncoding(const std::string & lhs, const std::string & rhs) {
        return (ucnv_compareNames(lhs.c_str(), rhs.c_str()) == 0);
    }

public:
    UnicodeConversionContext(
        const std::string & application_wide_char_encoding   = "UCS-2",
        const std::string & application_narrow_char_encoding = "UTF-8",
        const std::string & data_source_narrow_char_encoding = "UTF-8"
    )
        : skip_application_to_converter_pivot_wide_char_conversion (sameEncoding(application_wide_char_encoding, converter_pivot_wide_char_encoding))
        , skip_application_to_driver_pivot_narrow_char_conversion  (sameEncoding(application_narrow_char_encoding, driver_pivot_narrow_char_encoding))
        , skip_data_source_to_driver_pivot_narrow_char_conversion  (sameEncoding(data_source_narrow_char_encoding, driver_pivot_narrow_char_encoding))

        , application_wide_char_converter    (createConverter(application_wide_char_encoding))
        , application_narrow_char_converter  (createConverter(application_narrow_char_encoding))
        , data_source_narrow_char_converter  (createConverter(data_source_narrow_char_encoding))
        , driver_pivot_narrow_char_converter (createConverter(driver_pivot_narrow_char_encoding))
    {
    }

    ~UnicodeConversionContext() {
        destroyConverter(driver_pivot_narrow_char_converter);
        destroyConverter(data_source_narrow_char_converter);
        destroyConverter(application_narrow_char_converter);
        destroyConverter(application_wide_char_converter);
    }

public:
    const bool skip_application_to_converter_pivot_wide_char_conversion = false;
    const bool skip_application_to_driver_pivot_narrow_char_conversion  = false;
    const bool skip_data_source_to_driver_pivot_narrow_char_conversion  = false;

    UConverter * application_wide_char_converter    = nullptr;
    UConverter * application_narrow_char_converter  = nullptr;
    UConverter * data_source_narrow_char_converter  = nullptr;
    UConverter * driver_pivot_narrow_char_converter = nullptr;

public:
    template <typename CharType>
    inline void retireString(std::basic_string<CharType> && str);

    template <typename CharType>
    inline std::basic_string<CharType> allocateString();

private:
    template <typename CharType>
    inline ObjectPool<std::basic_string<CharType>> & accessStringPool(); // Leave unimplemented for general case.

    ObjectPool< std::basic_string<char>           > string_pool_c_   {10};
    ObjectPool< std::basic_string<signed char>    > string_pool_sc_  {10};
    ObjectPool< std::basic_string<unsigned char>  > string_pool_uc_  {10};
//  ObjectPool< std::basic_string<char8_t>        > string_pool_c8_  {10};
    ObjectPool< std::basic_string<char16_t>       > string_pool_c16_ {10};
    ObjectPool< std::basic_string<char32_t>       > string_pool_c32_ {10};
    ObjectPool< std::basic_string<wchar_t>        > string_pool_wc_  {10};
    ObjectPool< std::basic_string<unsigned short> > string_pool_us_  {10};
};

template <typename CharType>
inline void UnicodeConversionContext::retireString(std::basic_string<CharType> && str) {
    return accessStringPool<CharType>().put(std::move(str));
}

template <typename CharType>
inline std::basic_string<CharType> UnicodeConversionContext::allocateString() {
    return accessStringPool<CharType>().get();
}

template <>
inline ObjectPool<std::basic_string<char>> & UnicodeConversionContext::accessStringPool<char>() {
    return string_pool_c_;
}

template <>
inline ObjectPool<std::basic_string<signed char>> & UnicodeConversionContext::accessStringPool<signed char>() {
    return string_pool_sc_;
}

template <>
inline ObjectPool<std::basic_string<unsigned char>> & UnicodeConversionContext::accessStringPool<unsigned char>() {
    return string_pool_uc_;
}

/*
template <>
inline ObjectPool<std::basic_string<char8_t>> & UnicodeConversionContext::accessStringPool<char8_t>() {
    return string_pool_c8_;
}
*/

template <>
inline ObjectPool<std::basic_string<char16_t>> & UnicodeConversionContext::accessStringPool<char16_t>() {
    return string_pool_c16_;
}

template <>
inline ObjectPool<std::basic_string<char32_t>> & UnicodeConversionContext::accessStringPool<char32_t>() {
    return string_pool_c32_;
}

template <>
inline ObjectPool<std::basic_string<wchar_t>> & UnicodeConversionContext::accessStringPool<wchar_t>() {
    return string_pool_wc_;
}

template <>
inline ObjectPool<std::basic_string<unsigned short>> & UnicodeConversionContext::accessStringPool<unsigned short>() {
    return string_pool_us_;
}

template <typename SourceCharType, typename PivotCharType>
void convertEncodingToPivotRestricted(
    UConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot
) {
    // Check, that:
    //   1) converter reads source characters that are bit-compatible to SourceCharType,
    //   2) converter writes pivot characters that are bit-compatible to PivotCharType.

    static_assert(sizeof(PivotCharType) == sizeof(ConverterPivotCharType),
        "unable to convert encoding while writing pivot characters to the provided buffer");

    if (sizeof(SourceCharType) != ucnv_getMinCharSize(&src_converter))
        throw std::runtime_error("unable to convert encoding while reading source characters from the provided buffer");

    try {
        constexpr std::size_t extra_size_reserve = 32;
        constexpr std::size_t min_size_increment = 128;

        pivot.clear();
        pivot.resize(std::min(src.size() + extra_size_reserve, min_size_increment)); // TODO: replace with resize_without_initialization(pivot, ...)

        auto * source = reinterpret_cast<const char *>(src.data());
        auto * source_end = reinterpret_cast<const char *>(src.data() + src.size());

        auto * target = const_cast<ConverterPivotCharType *>(reinterpret_cast<const ConverterPivotCharType *>(pivot.c_str()));
        auto * target_end = reinterpret_cast<const ConverterPivotCharType *>(pivot.c_str() + pivot.size());

        std::size_t target_symbols_written = 0; // Note, that sizeof(PivotCharType) == sizeof(ConverterPivotCharType).

        while (true) {
            auto * target_prev = target;
            UErrorCode error_code = U_ZERO_ERROR;

            ucnv_toUnicode(&src_converter, &target, target_end, &source, source_end, nullptr, true, &error_code);

            target_symbols_written += target - target_prev;

            if (error_code == U_BUFFER_OVERFLOW_ERROR) {
                const std::size_t source_pending_bytes = source_end - source;
                const std::size_t source_pending_symbols = (source_pending_bytes / sizeof(SourceCharType)) + (source_pending_bytes % sizeof(SourceCharType) > 0 ? 1 : 0);
                const std::size_t target_new_size = target_symbols_written + source_pending_symbols;

                pivot.resize(std::min(target_new_size + extra_size_reserve, min_size_increment)); // TODO: replace with resize_without_initialization(pivot, ...)

                target = const_cast<ConverterPivotCharType *>(reinterpret_cast<const ConverterPivotCharType *>(pivot.c_str())) + target_symbols_written;
                target_end = reinterpret_cast<const ConverterPivotCharType *>(pivot.c_str() + pivot.size());
            }
            else if (U_FAILURE(error_code)) {
                throw std::runtime_error(u_errorName(error_code));
            }
            else {
                pivot.resize(target_symbols_written);
                break;
            }
        }
    }
    catch (...) {
        ucnv_resetToUnicode(&src_converter);
        pivot.clear();
        throw;
    }
}

template <typename ProxyCharType, typename SourceCharType, typename PivotCharType>
void convertEncodingToPivotViaProxyPivot(
    UConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot, UnicodeConversionContext & context
) {
    auto pivot_proxy = context.allocateString<ProxyCharType>();
    convertEncodingToPivot(src_converter, src, pivot_proxy, context);
    pivot.clear();
    pivot.reserve(pivot_proxy.size());
    pivot.assign(pivot_proxy.begin(), pivot_proxy.end());
    context.retireString(std::move(pivot_proxy));
}

template <typename ProxyCharType, typename SourceCharType, typename PivotCharType>
void convertEncodingToPivotRestrictedViaProxySource(
    UConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot, UnicodeConversionContext & context
) {
    auto src_proxy = context.allocateString<ProxyCharType>();
    src_proxy.clear();
    src_proxy.reserve(src.size());
    src_proxy.assign(src.begin(), src.end());
    convertEncodingToPivotRestricted(src_converter, std::basic_string_view<ProxyCharType>{src_proxy.c_str(), src_proxy.size()}, pivot);
    context.retireString(std::move(src_proxy));
}

template <typename SourceCharType, typename PivotCharType>
void convertEncodingToPivot(
    UConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot, UnicodeConversionContext & context
) {
    constexpr auto expected_pivot_char_size = sizeof(ConverterPivotCharType);

    if constexpr (expected_pivot_char_size != sizeof(PivotCharType)) {
        if constexpr (expected_pivot_char_size == 1)      return convertEncodingToPivotViaProxyPivot<char>(src_converter, src, pivot, context);
        else if constexpr (expected_pivot_char_size == 2) return convertEncodingToPivotViaProxyPivot<char16_t>(src_converter, src, pivot, context);
        else if constexpr (expected_pivot_char_size == 4) return convertEncodingToPivotViaProxyPivot<char32_t>(src_converter, src, pivot, context);
        else static_assert(dependent_false<PivotCharType>::value, "unable to convert encoding while writing pivot characters to the provided buffer");
    }

    const auto expected_src_char_size = static_cast<std::size_t>(ucnv_getMinCharSize(&src_converter));

    if (expected_src_char_size == sizeof(SourceCharType))
        return convertEncodingToPivotRestricted(src_converter, src, pivot);
    else switch (expected_src_char_size) {
        case 1: return convertEncodingToPivotRestrictedViaProxySource<char>(src_converter, src, pivot, context);
        case 2: return convertEncodingToPivotRestrictedViaProxySource<char16_t>(src_converter, src, pivot, context);
        case 4: return convertEncodingToPivotRestrictedViaProxySource<char32_t>(src_converter, src, pivot, context);
        default: throw std::runtime_error("unable to convert encoding while reading source characters from the provided buffer");
    }
}

template <typename SourceCharType, typename PivotCharType>
void convertEncodingToPivot(
    UConverter & src_converter, const std::basic_string<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot, UnicodeConversionContext & context
) {
    return convertEncodingToPivot(src_converter, std::basic_string_view<SourceCharType>{src.c_str(), src.size()}, pivot, context);
}

template <typename DestinationCharType, typename PivotCharType>
void convertEncodingFromPivotRestricted(
    const std::basic_string_view<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest
) {
    // Check, that:
    //   1) converter reads pivot characters that are bit-compatible to PivotCharType,
    //   2) converter writes destination characters that are bit-compatible to DestinationCharType.

    static_assert(sizeof(PivotCharType) == sizeof(ConverterPivotCharType),
        "unable to convert encoding while reading pivot characters from the provided buffer");

    if (sizeof(DestinationCharType) != ucnv_getMinCharSize(&dest_converter))
        throw std::runtime_error("unable to convert encoding while writing destination characters to the provided buffer");

    try {
        constexpr std::size_t extra_size_reserve = 32;
        constexpr std::size_t min_size_increment = 128;

        dest.clear();
        dest.resize(std::min(pivot.size() + extra_size_reserve, min_size_increment)); // TODO: replace with resize_without_initialization(dest, ...)

        auto * source = reinterpret_cast<const ConverterPivotCharType *>(pivot.data());
        auto * source_end = reinterpret_cast<const ConverterPivotCharType *>(pivot.data() + pivot.size());

        auto * target = const_cast<char *>(reinterpret_cast<const char *>(dest.c_str()));
        auto * target_end = reinterpret_cast<const char *>(dest.c_str() + dest.size());

        std::size_t target_bytes_written = 0;

        while (true) {
            auto * target_prev = target;
            UErrorCode error_code = U_ZERO_ERROR;

            ucnv_fromUnicode(&dest_converter, &target, target_end, &source, source_end, nullptr, true, &error_code);

            target_bytes_written += target - target_prev;

            if (error_code == U_BUFFER_OVERFLOW_ERROR) {
                const std::size_t source_pending_symbols = source_end - source;
                const std::size_t target_symbols_written = (target_bytes_written / sizeof(DestinationCharType)) + (target_bytes_written % sizeof(DestinationCharType) > 0 ? 1 : 0);
                const std::size_t target_new_size = target_symbols_written + source_pending_symbols;

                dest.resize(std::min(target_new_size + extra_size_reserve, min_size_increment)); // TODO: replace with resize_without_initialization(dest, ...)

                target = const_cast<char *>(reinterpret_cast<const char *>(dest.c_str())) + target_bytes_written;
                target_end = reinterpret_cast<const char *>(dest.c_str() + dest.size());
            }
            else if (U_FAILURE(error_code)) {
                throw std::runtime_error(u_errorName(error_code));
            }
            else {
                if (target_bytes_written % sizeof(DestinationCharType) != 0)
                    throw std::runtime_error("ucnv_fromUnicode() failed to write destination buffer to symbol boundary");

                dest.resize(target_bytes_written / sizeof(DestinationCharType));
                break;
            }
        }
    }
    catch (...) {
        ucnv_resetFromUnicode(&dest_converter);
        dest.clear();
        throw;
    }
}

template <typename ProxyCharType, typename DestinationCharType, typename PivotCharType>
void convertEncodingFromPivotViaProxyPivot(
    const std::basic_string_view<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest, UnicodeConversionContext & context
) {
    auto pivot_proxy = context.allocateString<ProxyCharType>();
    pivot_proxy.clear();
    pivot_proxy.reserve(pivot.size());
    pivot_proxy.assign(pivot.begin(), pivot.end());
    convertEncodingFromPivot(std::basic_string_view<ProxyCharType>{pivot_proxy.c_str(), pivot_proxy.size()}, dest_converter, dest, context);
    context.retireString(std::move(pivot_proxy));
}

template <typename ProxyCharType, typename DestinationCharType, typename PivotCharType>
void convertEncodingFromPivotRestrictedViaProxyDestination(
    const std::basic_string_view<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest, UnicodeConversionContext & context
) {
    auto dest_proxy = context.allocateString<ProxyCharType>();
    convertEncodingFromPivotRestricted(pivot, dest_converter, dest_proxy);
    dest.clear();
    dest.reserve(dest_proxy.size());
    dest.assign(dest_proxy.begin(), dest_proxy.end());
    context.retireString(std::move(dest_proxy));
}

template <typename DestinationCharType, typename PivotCharType>
void convertEncodingFromPivot(
    const std::basic_string_view<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest, UnicodeConversionContext & context
) {
    constexpr auto expected_pivot_char_size = sizeof(ConverterPivotCharType);

    if constexpr (expected_pivot_char_size != sizeof(PivotCharType)) {
        if constexpr (expected_pivot_char_size == 1)      return convertEncodingFromPivotViaProxyPivot<char>(pivot, dest_converter, dest, context);
        else if constexpr (expected_pivot_char_size == 2) return convertEncodingFromPivotViaProxyPivot<char16_t>(pivot, dest_converter, dest, context);
        else if constexpr (expected_pivot_char_size == 4) return convertEncodingFromPivotViaProxyPivot<char32_t>(pivot, dest_converter, dest, context);
        else static_assert(dependent_false<PivotCharType>::value, "unable to convert encoding while reading pivot characters from the provided buffer");
    }

    const auto expected_dest_char_size = static_cast<std::size_t>(ucnv_getMinCharSize(&dest_converter));

    if (expected_dest_char_size == sizeof(DestinationCharType))
        return convertEncodingFromPivotRestricted(pivot, dest_converter, dest);
    else switch (expected_dest_char_size) {
        case 1: return convertEncodingFromPivotRestrictedViaProxyDestination<char>(pivot, dest_converter, dest, context);
        case 2: return convertEncodingFromPivotRestrictedViaProxyDestination<char16_t>(pivot, dest_converter, dest, context);
        case 4: return convertEncodingFromPivotRestrictedViaProxyDestination<char32_t>(pivot, dest_converter, dest, context);
        default: throw std::runtime_error("unable to convert encoding while writing destination characters to the provided buffer");
    }
}

template <typename DestinationCharType, typename PivotCharType>
void convertEncodingFromPivot(
    const std::basic_string<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest, UnicodeConversionContext & context
) {
    return convertEncodingFromPivot(std::basic_string_view<PivotCharType>{pivot.c_str(), pivot.size()}, dest_converter, dest, context);
}

template <typename SourceCharType, typename DestinationCharType, typename PivotCharType>
void convertEncoding(
    UConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest,
    UnicodeConversionContext & context
) {
    convertEncodingToPivot(src_converter, src, pivot, context);
    convertEncodingFromPivot(pivot, dest_converter, dest, context);
}

template <typename SourceCharType, typename DestinationCharType, typename PivotCharType>
void convertEncoding(
    UConverter & src_converter, const std::basic_string<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest,
    UnicodeConversionContext & context
) {
    return convertEncoding(src_converter, std::basic_string_view<SourceCharType>{src.c_str(), src.size()}, pivot, dest_converter, dest, context);
}

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

// StringLength() - return the number of characters in the string assuming the encoding of the provided converter.

template <typename CharType>
inline std::size_t StringLength(const std::basic_string_view<CharType> str, UConverter & converter, UnicodeConversionContext & context) {
    auto pivot = context.allocateString<ConverterPivotCharType>();
    convertEncodingToPivot(converter, str, pivot, context);
    const auto pivot_no_bom = ConsumeBOMUTF16(std::basic_string_view<ConverterPivotCharType>{pivot.c_str(), pivot.size()});
    const auto len = u_countChar32(pivot_no_bom.data(), pivot_no_bom.size());
    context.retireString(std::move(pivot));
    return (len > 0 ? len : 0);
}

template <typename CharType>
inline std::size_t StringLength(const std::basic_string<CharType> str, UConverter & converter, UnicodeConversionContext & context) {
    return StringLength(std::basic_string_view<CharType>{str, str.size()}, converter, context);
}

template <typename CharType>
inline std::size_t StringLength(const CharType * str, const std::size_t size, UConverter & converter, UnicodeConversionContext & context) {
    if (!str || !size)
        return 0;

    return StringLength(std::basic_string_view<CharType>{str, size}, converter, context);
}

template <typename CharType>
inline std::size_t StringLength(const CharType * str, UConverter & converter, UnicodeConversionContext & context) {
    return StringLength(str, StringBufferLength(str), converter, context);
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
    return StringLengthUTF8(std::basic_string_view<CharType>{str, str.size()});
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

// toUTF8() - convert a string from application encoding to driver's pivot encoding (UTF-8).

template <typename CharType>
inline std::basic_string<DriverPivotCharType> toUTF8(const std::basic_string_view<CharType> & src, UnicodeConversionContext & context) {
    auto result = context.allocateString<DriverPivotCharType>();
    result.clear();

    if constexpr (sizeof(CharType) == sizeof(DriverPivotCharType)) {
        if (context.skip_application_to_driver_pivot_narrow_char_conversion) {
            resize_without_initialization(result, src.size());
            std::memcpy(&result[0], &src[0], src.size() * sizeof(CharType));
        }
        else {
            auto pivot = context.allocateString<ConverterPivotCharType>();
            convertEncoding(*context.application_narrow_char_converter, src, pivot, *context.driver_pivot_narrow_char_converter, result, context);
            context.retireString(std::move(pivot));
        }
    }
    else {
        if (context.skip_application_to_converter_pivot_wide_char_conversion) {
            convertEncodingFromPivot(src, *context.driver_pivot_narrow_char_converter, result, context);
        }
        else {
            auto pivot = context.allocateString<ConverterPivotCharType>();
            convertEncoding(*context.application_wide_char_converter, src, pivot, *context.driver_pivot_narrow_char_converter, result, context);
            context.retireString(std::move(pivot));
        }
    }

    return result;
}

template <typename CharType>
inline std::basic_string<DriverPivotCharType> toUTF8(const std::basic_string_view<CharType> & src) {
    UnicodeConversionContext context;
    return toUTF8(src, context);
}

template <typename CharType>
inline std::basic_string<DriverPivotCharType> toUTF8(const std::basic_string<CharType> & src, UnicodeConversionContext & context) {
    return toUTF8(std::basic_string_view<CharType>{src.c_str(), src.size()}, context);
}

template <typename CharType>
inline std::basic_string<DriverPivotCharType> toUTF8(const std::basic_string<CharType> & src) {
    return toUTF8(std::basic_string_view<CharType>{src.c_str(), src.size()});
}

template <typename CharType>
inline std::basic_string<DriverPivotCharType> toUTF8(const CharType * src, SQLLEN length, UnicodeConversionContext & context) {
    if (!src || (length != SQL_NTS && length <= 0))
        return toUTF8(std::basic_string_view<CharType>{}, context);

    // Workaround for UnixODBC Unicode client vs ANSI driver string encoding issue:
    // strings may be reported with a fixed length that also includes a trailing null character.
    // TODO: review this. This is not a formally right thing to do, but should not cause problems in practice.
#if defined(WORKAROUND_ENABLE_TRIM_TRAILING_NULL)
    if constexpr (std::is_same_v<CharType, char>) {
        if (src && length > 0 && src[length - 1] == '\0')
            --length;
    }
#endif

    return toUTF8(
        (
            length == SQL_NTS ?
            std::basic_string_view<CharType>{src} :
            std::basic_string_view<CharType>{src, static_cast<std::size_t>(length)}
        ),
        context
    );
}

template <typename CharType>
inline std::basic_string<DriverPivotCharType> toUTF8(const CharType * src, UnicodeConversionContext & context) {
    return toUTF8(src, SQL_NTS, context);
}

template <typename CharType>
inline std::basic_string<DriverPivotCharType> toUTF8(const CharType * src, SQLLEN length = SQL_NTS) {
    UnicodeConversionContext context;
    return toUTF8(src, length, context);
}


// fromUTF8() - convert a string from driver's pivot encoding (UTF-8) to application encoding.

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const std::basic_string_view<DriverPivotCharType> & src, UnicodeConversionContext & context) {
    auto result = context.allocateString<CharType>();
    result.clear();

    if constexpr (sizeof(CharType) == sizeof(DriverPivotCharType)) {
        if (context.skip_application_to_driver_pivot_narrow_char_conversion) {
            result.resize(src.size()); // TODO: replace with resize_without_initialization(result, src.size());
            std::memcpy(&result[0], &src[0], src.size() * sizeof(CharType));
        }
        else {
            auto pivot = context.allocateString<ConverterPivotCharType>();
            convertEncoding(*context.driver_pivot_narrow_char_converter, src, pivot, *context.application_narrow_char_converter, result, context);
            context.retireString(std::move(pivot));
        }
    }
    else {
        if (context.skip_application_to_converter_pivot_wide_char_conversion) {
            if constexpr (sizeof(CharType) == sizeof(ConverterPivotCharType)) {
                convertEncodingToPivot(*context.driver_pivot_narrow_char_converter, src, result, context);
            }
            else {
                auto pivot = context.allocateString<ConverterPivotCharType>();
                convertEncoding(*context.driver_pivot_narrow_char_converter, src, pivot, *context.application_wide_char_converter, result, context);
                context.retireString(std::move(pivot));
            }
        }
        else {
            auto pivot = context.allocateString<ConverterPivotCharType>();
            convertEncoding(*context.driver_pivot_narrow_char_converter, src, pivot, *context.application_wide_char_converter, result, context);
            context.retireString(std::move(pivot));
        }
    }

    return result;
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const std::basic_string_view<DriverPivotCharType> & src) {
    UnicodeConversionContext context;
    return fromUTF8<CharType>(src, context);
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const std::basic_string<DriverPivotCharType> & src, UnicodeConversionContext & context) {
    return fromUTF8<CharType>(std::basic_string_view<DriverPivotCharType>{src.c_str(), src.size()}, context);
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const std::basic_string<DriverPivotCharType> & src) {
    return fromUTF8<CharType>(std::basic_string_view<DriverPivotCharType>{src.c_str(), src.size()});
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const char * src, SQLLEN length, UnicodeConversionContext & context) {
    if (!src || (length != SQL_NTS && length <= 0))
        return fromUTF8<CharType>(std::basic_string_view<DriverPivotCharType>{}, context);

    return fromUTF8<CharType>(
        (
            length == SQL_NTS ?
            std::basic_string_view<DriverPivotCharType>{src} :
            std::basic_string_view<DriverPivotCharType>{src, static_cast<std::size_t>(length)}
        ),
        context
    );
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const char * src, UnicodeConversionContext & context) {
    return fromUTF8<CharType>(src, SQL_NTS, context);
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const char * src, SQLLEN length = SQL_NTS) {
    UnicodeConversionContext context;
    return fromUTF8<CharType>(src, length, context);
}

template <typename CharType>
inline void fromUTF8(const std::basic_string<DriverPivotCharType> & src, std::basic_string<CharType> & dest) {
    dest = fromUTF8<CharType>(src);
}
