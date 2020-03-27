#pragma once

#include "driver/utils/object_pool.h"
#include "driver/utils/resize_without_initialization.h"

#include <unicode/ustring.h>
#include <unicode/ucnv.h>

//FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(char)
FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(signed char)
FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(unsigned char)
//FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(char8_t)
FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(char16_t)
FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(char32_t)
//FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(wchar_t)
FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(unsigned short)

using ConverterPivotCharType = UChar;
using DriverPivotCharType = char;

inline std::string make_FEFF_raw_str() {
    ConverterPivotCharType ch = 0xFEFF;
    return {
        reinterpret_cast<const char *>(&ch),
        sizeof(ConverterPivotCharType)
    };
}

class UnicodeConversionContext {
public:
    inline static const std::string converter_pivot_wide_char_encoding = "UTF-16";
    inline static const std::string driver_pivot_narrow_char_encoding = "UTF-8";

    inline static const std::string converter_pivot_wide_char_signature = make_FEFF_raw_str();

private:
    inline static UConverter * createConverter(const std::string & ecnoding);
    inline static void destroyConverter(UConverter * & converter) noexcept;
    inline static bool sameEncoding(const std::string & lhs, const std::string & rhs);
    inline static std::string detectSignature(UConverter & converter);

public:
    UnicodeConversionContext(
#if defined(_win_)
        const std::string & application_wide_char_encoding   = (sizeof(SQLWCHAR) == 4 ? "UTF-32" : "UTF-16"),
#else
        const std::string & application_wide_char_encoding   = (sizeof(SQLWCHAR) == 4 ? "UCS-4" : "UCS-2"),
#endif
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

        , application_wide_char_signature    (detectSignature(*application_wide_char_converter))
        , application_narrow_char_signature  (detectSignature(*application_narrow_char_converter))
        , data_source_narrow_char_signature  (detectSignature(*data_source_narrow_char_converter))
        , driver_pivot_narrow_char_signature (detectSignature(*driver_pivot_narrow_char_converter))
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

    // Detected signature buffers, a.k.a, BOM - byte order mark.
    const std::string application_wide_char_signature;
    const std::string application_narrow_char_signature;
    const std::string data_source_narrow_char_signature;
    const std::string driver_pivot_narrow_char_signature;

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
void convertEncodingToPivot(
    UConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot,
    const std::string_view & signature_to_trim = {}
) {
    static_assert(sizeof(PivotCharType) == sizeof(ConverterPivotCharType), "unable to convert encoding: unsuitable character type for the pivot");

    if (sizeof(SourceCharType) != ucnv_getMinCharSize(&src_converter))
        throw std::runtime_error("unable to convert encoding: unsuitable character type for the source converter");

    try {
        constexpr std::size_t extra_size_reserve = 32;
        constexpr std::size_t min_size_increment = 128;

        pivot.clear();
        const auto initial_new_size = std::min(src.size() + extra_size_reserve, min_size_increment);
        resize_without_initialization(pivot, initial_new_size);

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

                const auto final_new_size = std::max(target_new_size + extra_size_reserve, min_size_increment);
                resize_without_initialization(pivot, final_new_size);

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

        ConsumeSignature(pivot, signature_to_trim);
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
    std::basic_string<PivotCharType> & pivot,
    const std::string_view & signature_to_trim = {}
) {
    return convertEncodingToPivot(src_converter, make_string_view(src), pivot, signature_to_trim);
}

template <typename DestinationCharType, typename PivotCharType>
void convertEncodingFromPivot(
    const std::basic_string_view<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest,
    const std::string_view & signature_to_trim = {}
) {
    static_assert(sizeof(PivotCharType) == sizeof(ConverterPivotCharType), "unable to convert encoding: unsuitable character type for the pivot");

    if (sizeof(DestinationCharType) != ucnv_getMinCharSize(&dest_converter))
        throw std::runtime_error("unable to convert encoding: unsuitable character type for the destination converter");

    try {
        constexpr std::size_t extra_size_reserve = 32;
        constexpr std::size_t min_size_increment = 128;

        dest.clear();
        const auto initial_new_size = std::min(pivot.size() + extra_size_reserve, min_size_increment);
        resize_without_initialization(dest, initial_new_size);

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

                const auto final_new_size = std::max(target_new_size + extra_size_reserve, min_size_increment);
                resize_without_initialization(dest, final_new_size);

                target = const_cast<char *>(reinterpret_cast<const char *>(dest.c_str())) + target_bytes_written;
                target_end = reinterpret_cast<const char *>(dest.c_str() + dest.size());
            }
            else if (U_FAILURE(error_code)) {
                throw std::runtime_error(u_errorName(error_code));
            }
            else {
                if (target_bytes_written % sizeof(DestinationCharType) != 0)
                    throw std::runtime_error("unable to convert encoding: ucnv_fromUnicode() failed to write destination buffer to symbol boundary");

                dest.resize(target_bytes_written / sizeof(DestinationCharType));
                break;
            }
        }

        ConsumeSignature(dest, signature_to_trim);
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
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest,
    const std::string_view & signature_to_trim = {}
) {
    return convertEncodingFromPivot(make_string_view(pivot), dest_converter, dest, signature_to_trim);
}

template <typename SourceCharType, typename DestinationCharType, typename PivotCharType>
void convertEncoding(
    UConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest,
    const std::string_view & signature_to_trim
) {
    convertEncodingToPivot(src_converter, src, pivot);
    convertEncodingFromPivot(pivot, dest_converter, dest, signature_to_trim);
}

template <typename SourceCharType, typename DestinationCharType, typename PivotCharType>
void convertEncoding(
    UConverter & src_converter, const std::basic_string<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot,
    UConverter & dest_converter, std::basic_string<DestinationCharType> & dest,
    const std::string_view & signature_to_trim
) {
    return convertEncoding(src_converter, make_string_view(src), pivot, dest_converter, dest, signature_to_trim);
}


// StringLength() - return the number of characters in the string assuming the encoding of the provided converter.

template <typename CharType>
inline std::size_t StringLength(const std::basic_string_view<CharType> & str, UConverter & converter, UnicodeConversionContext & context) {
    auto pivot = context.allocateString<ConverterPivotCharType>();
    convertEncodingToPivot(converter, str, pivot, context.converter_pivot_wide_char_signature);
    const auto len = u_countChar32(pivot.c_str(), pivot.size());
    context.retireString(std::move(pivot));
    return (len > 0 ? len : 0);
}

template <typename CharType>
inline std::size_t StringLength(const std::basic_string<CharType> & str, UConverter & converter, UnicodeConversionContext & context) {
    return StringLength(make_string_view(str), converter, context);
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


// toUTF8() - convert a string from application encoding to driver's pivot encoding (UTF-8).

template <typename CharType>
inline std::basic_string<DriverPivotCharType> toUTF8(const std::basic_string_view<CharType> & src, UnicodeConversionContext & context) {
    auto result = context.allocateString<DriverPivotCharType>();
    result.clear();

    if constexpr (sizeof(CharType) == sizeof(DriverPivotCharType)) {
        if (context.skip_application_to_driver_pivot_narrow_char_conversion) {
            auto src_no_sig = ConsumeSignature(src, context.driver_pivot_narrow_char_signature);
            resize_without_initialization(result, src_no_sig.size());
            std::memcpy(&result[0], &src_no_sig[0], src_no_sig.size() * sizeof(CharType));
        }
        else {
            auto pivot = context.allocateString<ConverterPivotCharType>();
            convertEncoding(*context.application_narrow_char_converter, src, pivot, *context.driver_pivot_narrow_char_converter, result, context.driver_pivot_narrow_char_signature);
            context.retireString(std::move(pivot));
        }
    }
    else if (
        context.skip_application_to_converter_pivot_wide_char_conversion &&
        sizeof(CharType) == sizeof(ConverterPivotCharType)
    ) {
        if constexpr (sizeof(CharType) == sizeof(ConverterPivotCharType)) // Re-check statically to avoid instantiation.
            convertEncodingFromPivot(src, *context.driver_pivot_narrow_char_converter, result, context.driver_pivot_narrow_char_signature);
    }
    else {
        auto pivot = context.allocateString<ConverterPivotCharType>();
        convertEncoding(*context.application_wide_char_converter, src, pivot, *context.driver_pivot_narrow_char_converter, result, context.driver_pivot_narrow_char_signature);
        context.retireString(std::move(pivot));
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
    return toUTF8(make_string_view(src), context);
}

template <typename CharType>
inline std::basic_string<DriverPivotCharType> toUTF8(const std::basic_string<CharType> & src) {
    return toUTF8(make_string_view(src));
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
            auto src_no_sig = ConsumeSignature(src, context.application_narrow_char_signature);
            resize_without_initialization(result, src_no_sig.size());
            std::memcpy(&result[0], &src_no_sig[0], src_no_sig.size() * sizeof(CharType));
        }
        else {
            auto pivot = context.allocateString<ConverterPivotCharType>();
            convertEncoding(*context.driver_pivot_narrow_char_converter, src, pivot, *context.application_narrow_char_converter, result, context.application_narrow_char_signature);
            context.retireString(std::move(pivot));
        }
    }
    else if (
        context.skip_application_to_converter_pivot_wide_char_conversion &&
        sizeof(CharType) == sizeof(ConverterPivotCharType)
    ) {
        if constexpr (sizeof(CharType) == sizeof(ConverterPivotCharType)) // Re-check statically to avoid instantiation.
            convertEncodingToPivot(*context.driver_pivot_narrow_char_converter, src, result, context.converter_pivot_wide_char_signature);
    }
    else {
        auto pivot = context.allocateString<ConverterPivotCharType>();
        convertEncoding(*context.driver_pivot_narrow_char_converter, src, pivot, *context.application_wide_char_converter, result, context.application_wide_char_signature);
        context.retireString(std::move(pivot));
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
    return fromUTF8<CharType>(make_string_view(src), context);
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const std::basic_string<DriverPivotCharType> & src) {
    return fromUTF8<CharType>(make_string_view(src));
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const DriverPivotCharType * src, SQLLEN length, UnicodeConversionContext & context) {
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
inline std::basic_string<CharType> fromUTF8(const DriverPivotCharType * src, UnicodeConversionContext & context) {
    return fromUTF8<CharType>(src, SQL_NTS, context);
}

template <typename CharType>
inline std::basic_string<CharType> fromUTF8(const DriverPivotCharType * src, SQLLEN length = SQL_NTS) {
    UnicodeConversionContext context;
    return fromUTF8<CharType>(src, length, context);
}

template <typename CharType>
inline void fromUTF8(const std::basic_string<DriverPivotCharType> & src, std::basic_string<CharType> & dest) {
    dest = fromUTF8<CharType>(src);
}


inline UConverter * UnicodeConversionContext::createConverter(const std::string & ecnoding) {
    UErrorCode error_code = U_ZERO_ERROR;
    UConverter * converter = ucnv_open(ecnoding.c_str(), &error_code);

    if (U_FAILURE(error_code))
        throw std::runtime_error(u_errorName(error_code));

    if (!converter)
        throw std::runtime_error("ucnv_open(" + ecnoding + ") failed");

    return converter;
}

inline void UnicodeConversionContext::destroyConverter(UConverter * & converter) noexcept {
    if (converter) {
        ucnv_close(converter);
        converter = nullptr;
    }
}

inline bool UnicodeConversionContext::sameEncoding(const std::string & lhs, const std::string & rhs) {
    return (ucnv_compareNames(lhs.c_str(), rhs.c_str()) == 0);
}

inline std::string UnicodeConversionContext::detectSignature(UConverter & converter) {
    std::basic_string<UChar> pivot;
    std::string dest;

    pivot.push_back(UChar(0x61)); // 'a'

    auto detect_signature = [&] (auto char_tag) {
        using DestinationCharType = std::decay_t<decltype(char_tag)>;

        std::basic_string<DestinationCharType> typed_dest;
        convertEncodingFromPivot(make_string_view(pivot), converter, typed_dest);
        dest.assign(reinterpret_cast<const char *>(typed_dest.c_str()), typed_dest.size() * sizeof(DestinationCharType));

        UErrorCode error_code = U_ZERO_ERROR;
        std::int32_t signature_length = 0;
        const auto * charset_name = ucnv_detectUnicodeSignature(dest.c_str(), dest.size(), &signature_length, &error_code);

        if (U_SUCCESS(error_code) && charset_name != nullptr && signature_length > 0 && signature_length < dest.size())
            dest.resize(signature_length);
        else // TODO: add some manual cases based on the known possible signatures for specific encodings?
            dest.clear();
    };

    const auto expected_dest_char_size = static_cast<std::size_t>(ucnv_getMinCharSize(&converter));

    switch (expected_dest_char_size) {
        case 1: detect_signature(char{});     break;
        case 2: detect_signature(char16_t{}); break;
        case 4: detect_signature(char32_t{}); break;
        default: throw std::runtime_error("unable to detect signature: unknown character type for the converter");
    }

    return dest;
}
