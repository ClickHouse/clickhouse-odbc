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
using DefaultPivotCharType = UChar;

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
        , skip_application_to_driver_narrow_char_conversion        (sameEncoding(application_narrow_char_encoding, driver_pivot_narrow_char_encoding))
        , skip_data_source_to_driver_narrow_char_conversion        (sameEncoding(data_source_narrow_char_encoding, driver_pivot_narrow_char_encoding))

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
    const bool skip_application_to_driver_narrow_char_conversion        = false;
    const bool skip_data_source_to_driver_narrow_char_conversion        = false;

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
    inline ObjectPool<std::basic_string<CharType>> & accessStringPool(void * = nullptr /* argument unused */); // Leave unimplemented for general case.

    ObjectPool< std::basic_string<char>                 > string_pool_c_    {3};
    ObjectPool< std::basic_string<signed char>          > string_pool_sc_   {3};
    ObjectPool< std::basic_string<unsigned char>        > string_pool_uc_   {3};
    ObjectPool< std::basic_string<char16_t>             > string_pool_c16_  {3};
    ObjectPool< std::basic_string<char32_t>             > string_pool_c32_  {3};
    ObjectPool< std::basic_string<wchar_t>              > string_pool_wc_   {3};
    ObjectPool< std::basic_string<unsigned short>       > string_pool_us_   {3};
    ObjectPool< std::basic_string<DefaultPivotCharType> > string_pool_Uc_   {3};
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
inline ObjectPool<std::basic_string<char>> & UnicodeConversionContext::accessStringPool<char>(void *) {
    return string_pool_c_;
}

template <>
inline ObjectPool<std::basic_string<signed char>> & UnicodeConversionContext::accessStringPool<signed char>(void *) {
    return string_pool_sc_;
}

template <>
inline ObjectPool<std::basic_string<unsigned char>> & UnicodeConversionContext::accessStringPool<unsigned char>(void *) {
    return string_pool_uc_;
}

template <>
inline ObjectPool<std::basic_string<char16_t>> & UnicodeConversionContext::accessStringPool<char16_t>(void *) {
    return string_pool_c16_;
}

template <>
inline ObjectPool<std::basic_string<char32_t>> & UnicodeConversionContext::accessStringPool<char32_t>(void *) {
    return string_pool_c32_;
}

template <>
inline ObjectPool<std::basic_string<wchar_t>> & UnicodeConversionContext::accessStringPool<wchar_t>(void *) {
    return string_pool_wc_;
}

template <>
inline ObjectPool<std::basic_string<unsigned short>> & UnicodeConversionContext::accessStringPool<unsigned short>(void *) {
    return string_pool_us_;
}

/*

template <>
inline ObjectPool<std::basic_string<DefaultPivotCharType>> & UnicodeConversionContext::accessStringPool<DefaultPivotCharType>(
    std::enable_if_t<
        !std::is_same_v<DefaultPivotCharType, unsigned short> &&
        !std::is_same_v<DefaultPivotCharType, char16_t>
    >*
) {
    return string_pool_Uc_;
}
*/

template <typename SourceCharType, typename PivotCharType>
void convertEncodingToPivot(
    UConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot
) {
    try {
        constexpr std::size_t extra_size_reserve = 32;
        constexpr std::size_t min_size_increment = 128;

        pivot.clear();
        pivot.resize(std::min(src.size() + extra_size_reserve, min_size_increment)); // TODO: replace with resize_without_initialization(pivot, ...)

        auto * source = reinterpret_cast<const char *>(src.data());
        auto * source_end = reinterpret_cast<const char *>(src.data() + src.size());

        static_assert(sizeof(PivotCharType) == sizeof(DefaultPivotCharType)); // ...and, hopefully, bit-compatible.

        auto * target = const_cast<DefaultPivotCharType *>(reinterpret_cast<const DefaultPivotCharType *>(pivot.c_str()));
        auto * target_end = reinterpret_cast<const DefaultPivotCharType *>(pivot.c_str() + pivot.size());

        std::size_t target_symbols_written = 0; // Note, that sizeof(PivotCharType) == sizeof(DefaultPivotCharType).

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

                target = const_cast<DefaultPivotCharType *>(reinterpret_cast<const DefaultPivotCharType *>(pivot.c_str())) + target_symbols_written;
                target_end = reinterpret_cast<const DefaultPivotCharType *>(pivot.c_str() + pivot.size());
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

template <typename SourceCharType, typename PivotCharType>
void convertEncodingToPivot(
    UConverter & src_converter, const std::basic_string<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot
) {
    return convertEncodingToPivot(src_converter, std::basic_string_view<SourceCharType>{src.c_str(), src.size()}, pivot);
}

template <typename DestinationCharType, typename PivotCharType>
void convertEncodingFromPivot(
    const std::basic_string_view<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest
) {
    try {
        constexpr std::size_t extra_size_reserve = 32;
        constexpr std::size_t min_size_increment = 128;

        dest.clear();
        dest.resize(std::min(pivot.size() + extra_size_reserve, min_size_increment)); // TODO: replace with resize_without_initialization(dest, ...)

        static_assert(sizeof(PivotCharType) == sizeof(DefaultPivotCharType)); // ...and, hopefully, bit-compatible.

        auto * source = reinterpret_cast<const DefaultPivotCharType *>(pivot.data());
        auto * source_end = reinterpret_cast<const DefaultPivotCharType *>(pivot.data() + pivot.size());

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

                dest.resize(target_bytes_written * sizeof(DestinationCharType));
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

template <typename DestinationCharType, typename PivotCharType>
void convertEncodingFromPivot(
    const std::basic_string<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest
) {
    return convertEncodingFromPivot(std::basic_string_view<PivotCharType>{pivot.c_str(), pivot.size()}, dest_converter, dest);
}

template <typename SourceCharType, typename DestinationCharType, typename PivotCharType>
void convertEncoding(
    UConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest
) {
    convertEncodingToPivot(src_converter, src, pivot);
    convertEncodingFromPivot(pivot, dest_converter, dest);
}

template <typename SourceCharType, typename DestinationCharType, typename PivotCharType>
void convertEncoding(
    UConverter & src_converter, const std::basic_string<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest
) {
    return convertEncoding(src_converter, std::basic_string_view<SourceCharType>{src.c_str(), src.size()}, pivot, dest_converter, dest);
}


// NTSBufferLength() - number of elements in the null-terminated buffer (that holds a string).

template <typename CharType>
inline std::size_t NTSBufferLength(const CharType * str) {
    return (str ? std::basic_string_view<CharType>{str}.size() + 1 : 0);
}


// NTSStringLength() - number of characters in the null-terminated string in application encoding.

template <typename CharType>
inline std::size_t NTSStringLength(const CharType * str, UnicodeConversionContext & context) {
    if (!str)
        return 0;

    if constexpr (sizeof(CharType) == sizeof(char)) {
        auto pivot = context.allocateString<DefaultPivotCharType>();
        convertEncodingToPivot(*context.application_narrow_char_converter, std::basic_string_view<CharType>{str}, pivot);
        const auto len = u_countChar32(pivot.c_str(), pivot.size());
        context.retireString(std::move(pivot));
        return (len > 0 ? len : 0);
    }
    else {
        if (context.skip_application_to_converter_pivot_wide_char_conversion) {
            const auto len = u_countChar32(reinterpret_cast<const DefaultPivotCharType *>(str), -1);
            return (len > 0 ? len : 0);
        }
        else {
            auto pivot = context.allocateString<DefaultPivotCharType>();
            convertEncodingToPivot(*context.application_wide_char_converter, std::basic_string_view<CharType>{str}, pivot);
            const auto len = u_countChar32(pivot.c_str(), pivot.size());
            context.retireString(std::move(pivot));
            return (len > 0 ? len : 0);
        }
    }
}

template <typename CharType>
inline std::size_t NTSStringLength(const CharType * str) {
    if (!str)
        return 0;

    UnicodeConversionContext context;
    return NTSStringLength(str, context);
}


// toUTF8() - convert a string from application encoding to driver's pivot encoding (UTF-8).

template <typename CharType>
inline std::string toUTF8(const std::basic_string_view<CharType> & src, UnicodeConversionContext & context) {
    auto result = context.allocateString<char>();
    result.clear();

    if constexpr (sizeof(CharType) == sizeof(char)) {
        if (context.skip_application_to_driver_narrow_char_conversion) {
            resize_without_initialization(result, src.size());
            std::memcpy(&result[0], &src[0], src.size());
        }
        else {
            auto pivot = context.allocateString<DefaultPivotCharType>();
            convertEncoding(*context.application_narrow_char_converter, src, pivot, *context.driver_pivot_narrow_char_converter, result);
            context.retireString(std::move(pivot));
        }
    }
    else {
        if (context.skip_application_to_converter_pivot_wide_char_conversion) {
            convertEncodingFromPivot(src, *context.driver_pivot_narrow_char_converter, result);
        }
        else {
            auto pivot = context.allocateString<DefaultPivotCharType>();
            convertEncoding(*context.application_wide_char_converter, src, pivot, *context.driver_pivot_narrow_char_converter, result);
            context.retireString(std::move(pivot));
        }
    }

    return result;
}

template <typename CharType>
inline std::string toUTF8(const std::basic_string_view<CharType> & src) {
    UnicodeConversionContext context;
    return toUTF8(src, context);
}

template <typename CharType>
inline std::string toUTF8(const std::basic_string<CharType> & src, UnicodeConversionContext & context) {
    return toUTF8(std::basic_string_view<CharType>{src.c_str(), src.size()}, context);
}

template <typename CharType>
inline std::string toUTF8(const std::basic_string<CharType> & src) {
    return toUTF8(std::basic_string_view<CharType>{src.c_str(), src.size()});
}

template <typename CharType>
inline std::string toUTF8(const CharType * src, SQLLEN length, UnicodeConversionContext & context) {
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
            std::basic_string_view<CharType>{src, static_cast<std::string::size_type>(length)}
        ),
        context
    );
}

template <typename CharType>
inline std::string toUTF8(const CharType * src, UnicodeConversionContext & context) {
    return toUTF8(src, SQL_NTS, context);
}

template <typename CharType>
inline std::string toUTF8(const CharType * src, SQLLEN length = SQL_NTS) {
    UnicodeConversionContext context;
    return toUTF8(src, length, context);
}


// fromUTF8() - convert a string from driver's pivot encoding (UTF-8) to application encoding.

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const std::string_view & src, UnicodeConversionContext & context) {
    auto result = context.allocateString<CharType>();
    result.clear();

    if constexpr (sizeof(CharType) == sizeof(char)) {
        if (context.skip_application_to_driver_narrow_char_conversion) {
            result.resize(src.size()); // TODO: replace with resize_without_initialization(result, src.size());
            std::memcpy(&result[0], &src[0], src.size());
        }
        else {
            auto pivot = context.allocateString<DefaultPivotCharType>();
            convertEncoding(*context.driver_pivot_narrow_char_converter, src, pivot, *context.application_narrow_char_converter, result);
            context.retireString(std::move(pivot));
        }
    }
    else {
        if (context.skip_application_to_converter_pivot_wide_char_conversion) {
            if constexpr (sizeof(CharType) == sizeof(DefaultPivotCharType)) {
                convertEncodingToPivot(*context.driver_pivot_narrow_char_converter, src, result);
            }
            else {
                auto pivot = context.allocateString<DefaultPivotCharType>();
                convertEncoding(*context.driver_pivot_narrow_char_converter, src, pivot, *context.application_wide_char_converter, result);
                context.retireString(std::move(pivot));
            }
        }
        else {
            auto pivot = context.allocateString<DefaultPivotCharType>();
            convertEncoding(*context.driver_pivot_narrow_char_converter, src, pivot, *context.application_wide_char_converter, result);
            context.retireString(std::move(pivot));
        }
    }

    return result;
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const std::string_view & src) {
    UnicodeConversionContext context;
    return fromUTF8<CharType>(src, context);
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const std::string & src, UnicodeConversionContext & context) {
    return fromUTF8<CharType>(std::string_view{src.c_str(), src.size()}, context);
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const std::string & src) {
    return fromUTF8<CharType>(std::string_view{src.c_str(), src.size()});
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const char * src, SQLLEN length, UnicodeConversionContext & context) {
    if (!src || (length != SQL_NTS && length <= 0))
        return fromUTF8<CharType>(std::string_view{}, context);

    return fromUTF8<CharType>(
        (
            length == SQL_NTS ?
            std::string_view{src} :
            std::string_view{src, static_cast<std::string::size_type>(length)}
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
inline void fromUTF8(const std::string & src, std::basic_string<CharType> & dest) {
    dest = fromUTF8<CharType>(src);
}
