#pragma once

#include "driver/utils/resize_without_initialization.h"
#include "driver/utils/conversion_context.h"

#include <unicode/ustring.h>
#include <unicode/ucnv.h>

#include <string>
#include <string_view>

#include <cstring>

template <typename> struct dependent_false : std::false_type {};

namespace value_manip {

    // from_X<X_StorageType>::to_Y<Y_StorageType>::convert(src, dest, context) converts src and writes result into dest,
    // assuming src has encoding of X and dest's encoding should be encoding of Y, for the corresponding char widths.
    //
    // For example, in Linux and UnixODBC builds, if:
    //   X is application,
    //   X_StorageType is SQLWCHAR *,
    //   Y is driver
    //   Y_StorageType is std::string
    // then, the content of src will be interpreted as wide-char string in UCS-16, converted to narrow-char UTF-8, and
    // the result will be written into dest. The corresponding converters will be taken from the provided context.
    //
    // The overloads allow differnet actual ways of providing src string:
    //   X_StorageType * buffer with explicitly provided string length
    //   X_StorageType * buffer of null-terminated string
    //   std::basic_string<X_StorageType>
    //   std::basic_string_view<X_StorageType>
    //
    // dest is always assumed to be of std::basic_string<Y_StorageType> type.

    template <typename SourceType>
    struct from_application {
        template <typename DestinationType>
        struct to_driver {
            static inline void convert(const SourceType & src, DestinationType & dest); // Leave unimplemented for general case.
        };
    };

    template <typename SourceType>
    struct from_driver {
        template <typename DestinationType>
        struct to_application {
            static inline void convert(const SourceType & src, DestinationType & dest); // Leave unimplemented for general case.
        };
    };

    template <typename SourceType>
    struct from_data_source {
        template <typename DestinationType>
        struct to_driver {
            static inline void convert(const SourceType & src, DestinationType & dest); // Leave unimplemented for general case.
        };
    };

    template <typename SourceCharType> // char or char16_t
    struct from_application<SourceCharType *> {
        template <typename DestinationType>
        struct to_driver {

            // Not allowed to specialize a member template of a partially specialized class, so perform this check instead.
            static_assert(std::is_same_v<DestinationType, std::basic_string<DriverPivotNarrowCharType>>);

            template <typename ConversionContext = DefaultConversionContext>
            static inline void convert(const std::basic_string_view<SourceCharType> & src, DestinationType & dest, ConversionContext && context = ConversionContext{}) {

                dest.clear();

                if (src.size() == 0)
                    return;

                if constexpr (sizeof(SourceCharType) == sizeof(ApplicationNarrowCharType)) {
                    if (
                        context.skip_application_to_driver_pivot_narrow_char_conversion &&
                        sizeof(SourceCharType) == sizeof(DriverPivotNarrowCharType)
                    ) {
                        auto src_no_sig = context.driver_pivot_narrow_char_converter.consumeEncodedSignature(src);
                        resize_without_initialization(dest, src_no_sig.size());
                        std::memcpy(&dest[0], &src_no_sig[0], src_no_sig.size() * sizeof(SourceCharType));
                    }
                    else {
                        auto pivot = context.string_pool.template allocateString<ConverterPivotWideCharType>();
                        convertEncoding(context.application_narrow_char_converter, src, pivot, context.driver_pivot_narrow_char_converter, dest);
                        context.string_pool.retireString(std::move(pivot));
                    }
                }
                else if constexpr (sizeof(SourceCharType) == sizeof(ApplicationWideCharType)) {
                    if (
                        context.skip_application_to_converter_pivot_wide_char_conversion &&
                        sizeof(SourceCharType) == sizeof(ConverterPivotWideCharType)
                    ) {
                        if constexpr (sizeof(SourceCharType) == sizeof(ConverterPivotWideCharType)) // Re-check statically to avoid instantiation.
                            context.driver_pivot_narrow_char_converter.convertFromPivot(src, dest, true, true);
                    }
                    else {
                        auto pivot = context.string_pool.template allocateString<ConverterPivotWideCharType>();
                        convertEncoding(context.application_wide_char_converter, src, pivot, context.driver_pivot_narrow_char_converter, dest);
                        context.string_pool.retireString(std::move(pivot));
                    }
                }
                else {
                    static_assert(dependent_false<SourceCharType>::value, "conversion not defined");
                }
            }

            template <typename ConversionContext = DefaultConversionContext>
            static inline void convert(const std::basic_string<SourceCharType> & src, DestinationType & dest, ConversionContext && context = ConversionContext{}) {
                return convert(make_string_view(src), dest, std::forward<ConversionContext>(context));
            }

            template <typename ConversionContext = DefaultConversionContext>
            static inline void convert(const SourceCharType * src, DestinationType & dest, ConversionContext && context = ConversionContext{}) {
                return convert((src ? make_string_view(src) : std::basic_string_view<SourceCharType>{}), dest, std::forward<ConversionContext>(context));
            }

            template <typename ConversionContext = DefaultConversionContext>
            static inline void convert(const SourceCharType * src, SQLLEN src_length, DestinationType & dest, ConversionContext && context = ConversionContext{}) {
                if (!src || (src_length != SQL_NTS && src_length <= 0))
                    return convert(std::basic_string_view<SourceCharType>{}, dest, std::forward<ConversionContext>(context));

                if (src_length == SQL_NTS)
                    return convert(make_string_view(src), dest, std::forward<ConversionContext>(context));

                // Workaround for UnixODBC Unicode client vs ANSI driver string encoding issue:
                // strings may be reported with a fixed length that also includes a trailing null character.
                // TODO: review this. This is not a formally right thing to do, but should not cause problems in practice.
#if defined(WORKAROUND_ENABLE_TRIM_TRAILING_NULL)
                if constexpr (sizeof(SourceCharType) == 1) {
                    if (src && src_length > 0 && src[src_length - 1] == '\0')
                        --src_length;
                }
#endif

                return convert(make_string_view(src, static_cast<std::size_t>(src_length)), dest, std::forward<ConversionContext>(context));
            }
        };
    };

    template <>
    struct from_driver<std::basic_string<DriverPivotNarrowCharType>> {
        using SourceType = std::basic_string<DriverPivotNarrowCharType>;

        template <typename DestinationType>
        struct to_application {
            static inline void convert(const SourceType & src, DestinationType & dest); // Leave unimplemented for general case.
        };
    };

    template <typename DestinationCharType> // char or char16_t
    struct from_driver<std::basic_string<DriverPivotNarrowCharType>>::to_application<DestinationCharType *> {
        using DestinationType = std::basic_string<DestinationCharType>;

        template <typename ConversionContext = DefaultConversionContext>
        static inline void convert(const std::basic_string_view<DriverPivotNarrowCharType> & src, DestinationType & dest, ConversionContext && context = ConversionContext{}) {
            dest.clear();

            if (src.size() == 0)
                return;

            if constexpr (sizeof(DestinationCharType) == sizeof(ApplicationNarrowCharType)) {
                if (
                    context.skip_application_to_driver_pivot_narrow_char_conversion &&
                    sizeof(DestinationCharType) == sizeof(DriverPivotNarrowCharType)
                ) {
                    auto src_no_sig = context.application_narrow_char_converter.consumeEncodedSignature(src);
                    resize_without_initialization(dest, src_no_sig.size());
                    std::memcpy(&dest[0], &src_no_sig[0], src_no_sig.size() * sizeof(DriverPivotNarrowCharType));
                }
                else {
                    auto pivot = context.string_pool.template allocateString<ConverterPivotWideCharType>();
                    convertEncoding(context.driver_pivot_narrow_char_converter, src, pivot, context.application_narrow_char_converter, dest);
                    context.string_pool.retireString(std::move(pivot));
                }
            }
            else if constexpr (sizeof(DestinationCharType) == sizeof(ApplicationWideCharType)) {
                if (
                    context.skip_application_to_converter_pivot_wide_char_conversion &&
                    sizeof(DestinationCharType) == sizeof(ConverterPivotWideCharType)
                ) {
                    if constexpr (sizeof(DestinationCharType) == sizeof(ConverterPivotWideCharType)) // Re-check statically to avoid instantiation.
                        context.driver_pivot_narrow_char_converter.convertToPivot(src, dest, true, true);
                }
                else {
                    auto pivot = context.string_pool.template allocateString<ConverterPivotWideCharType>();
                    convertEncoding(context.driver_pivot_narrow_char_converter, src, pivot, context.application_wide_char_converter, dest);
                    context.string_pool.retireString(std::move(pivot));
                }
            }
            else {
                static_assert(dependent_false<DestinationCharType>::value, "conversion not defined");
            }
        }

        template <typename ConversionContext = DefaultConversionContext>
        static inline void convert(const std::basic_string<DriverPivotNarrowCharType> & src, DestinationType & dest, ConversionContext && context = ConversionContext{}) {
            return convert(make_string_view(src), dest, std::forward<ConversionContext>(context));
        }

        template <typename ConversionContext = DefaultConversionContext>
        static inline void convert(const DriverPivotNarrowCharType * src, DestinationType & dest, ConversionContext && context = ConversionContext{}) {
            return convert((src ? make_string_view(src) : std::basic_string_view<DriverPivotNarrowCharType>{}), dest, std::forward<ConversionContext>(context));
        }

        template <typename ConversionContext = DefaultConversionContext>
        static inline void convert(const DriverPivotNarrowCharType * src, SQLLEN src_length, DestinationType & dest, ConversionContext && context = ConversionContext{}) {
            if (!src || (src_length != SQL_NTS && src_length <= 0))
                return convert(std::basic_string_view<DriverPivotNarrowCharType>{}, dest, std::forward<ConversionContext>(context));

            if (src_length == SQL_NTS)
                return convert(make_string_view(src), dest, std::forward<ConversionContext>(context));

            return convert(make_string_view(src, static_cast<std::size_t>(src_length)), dest, std::forward<ConversionContext>(context));
        }
    };

} // namespace value_manip

// stringLength() - return the number of characters in the string assuming the encoding of the provided converter.

template <typename CharType>
inline std::size_t stringLength(const std::basic_string_view<CharType> & str, UnicodeConverter & converter, UnicodeConversionContext & context) {
    auto pivot = context.string_pool.allocateString<ConverterPivotWideCharType>();
    converter.convertToPivot(str, pivot, true, true);
    const auto len = u_countChar32(pivot.c_str(), pivot.size());
    context.string_pool.retireString(std::move(pivot));
    return (len > 0 ? len : 0);
}

template <typename CharType>
inline std::size_t stringLength(const std::basic_string<CharType> & str, UnicodeConverter & converter, UnicodeConversionContext & context) {
    return stringLength(make_string_view(str), converter, context);
}

template <typename CharType>
inline std::size_t stringLength(const CharType * str, const std::size_t size, UnicodeConverter & converter, UnicodeConversionContext & context) {
    if (!str || !size)
        return 0;

    return stringLength(std::basic_string_view<CharType>{str, size}, converter, context);
}

template <typename CharType>
inline std::size_t stringLength(const CharType * str, UnicodeConverter & converter, UnicodeConversionContext & context) {
    return stringLength(str, stringBufferLength(str), converter, context);
}

// Return the number of characters in the UTF-8 string (assuming valid UTF-8).
inline std::size_t stringLengthUTF8(const std::string_view & str) {

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

    const auto str_no_sig = consumeSignature(str, signature);
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


// (legacy utility functions) toUTF8() - convert a string from application encoding to driver's pivot encoding (UTF-8).

template <typename CharType>
inline auto toUTF8(const std::basic_string_view<CharType> & src, UnicodeConversionContext & context) {
    auto dest = context.string_pool.allocateString<DriverPivotNarrowCharType>();
    value_manip::from_application<CharType *>::template to_driver<std::basic_string<DriverPivotNarrowCharType>>::convert(src, dest, context);
    return dest;
}

template <typename CharType>
inline auto toUTF8(const std::basic_string_view<CharType> & src) {
    UnicodeConversionContext context;
    return toUTF8(src, context);
}

template <typename CharType>
inline auto toUTF8(const std::basic_string<CharType> & src, UnicodeConversionContext & context) {
    return toUTF8(make_string_view(src), context);
}

template <typename CharType>
inline auto toUTF8(const std::basic_string<CharType> & src) {
    return toUTF8(make_string_view(src));
}

template <typename CharType>
inline auto toUTF8(const CharType * src, SQLLEN src_length, UnicodeConversionContext & context) {
    auto dest = context.string_pool.allocateString<DriverPivotNarrowCharType>();
    value_manip::from_application<CharType *>::template to_driver<std::basic_string<DriverPivotNarrowCharType>>::convert(src, src_length, dest, context);
    return dest;
}

template <>
inline auto toUTF8<SQLTCHAR>(const SQLTCHAR * src, SQLLEN src_length, UnicodeConversionContext & context) {
    static_assert(sizeof(SQLTCHAR) == sizeof(PTChar)); // Helps noticing stupid refactoring mistakes
    return toUTF8(reinterpret_cast<const PTChar*>(src), src_length, context);
}

template <typename CharType>
inline auto toUTF8(const CharType * src, UnicodeConversionContext & context) {
    return toUTF8(src, SQL_NTS, context);
}

template <typename CharType>
inline auto toUTF8(const CharType * src, SQLLEN length = SQL_NTS) {
    UnicodeConversionContext context;
    return toUTF8(src, length, context);
}


// (legacy utility functions) fromUTF8() - convert a string from driver's pivot encoding (UTF-8) to application encoding.

template <typename CharType>
inline auto fromUTF8(const std::basic_string_view<DriverPivotNarrowCharType> & src, UnicodeConversionContext & context) {
    auto dest = context.string_pool.allocateString<CharType>();
    value_manip::from_driver<std::basic_string<DriverPivotNarrowCharType>>::template to_application<CharType*>::convert(src, dest, context);
    return dest;
}

template <typename CharType>
inline auto fromUTF8(const std::basic_string_view<DriverPivotNarrowCharType> & src) {
    UnicodeConversionContext context;
    return fromUTF8<CharType>(src, context);
}

template <typename CharType>
inline auto fromUTF8(const std::basic_string<DriverPivotNarrowCharType> & src, UnicodeConversionContext & context) {
    return fromUTF8<CharType>(make_string_view(src), context);
}

template <typename CharType>
inline auto fromUTF8(const std::basic_string<DriverPivotNarrowCharType> & src) {
    return fromUTF8<CharType>(make_string_view(src));
}

template <typename CharType>
inline auto fromUTF8(const DriverPivotNarrowCharType * src, SQLLEN src_length, UnicodeConversionContext & context) {
    auto dest = context.string_pool.allocateString<CharType>();
    value_manip::from_driver<std::basic_string<DriverPivotNarrowCharType>>::template to_application<CharType*>::convert(src, src_length, dest, context);
    return dest;
}

template <typename CharType>
inline auto fromUTF8(const DriverPivotNarrowCharType * src, UnicodeConversionContext & context) {
    return fromUTF8<CharType>(src, SQL_NTS, context);
}

template <typename CharType>
inline auto fromUTF8(const DriverPivotNarrowCharType * src, SQLLEN length = SQL_NTS) {
    UnicodeConversionContext context;
    return fromUTF8<CharType>(src, length, context);
}

template <typename CharType>
inline void fromUTF8(const std::basic_string_view<DriverPivotNarrowCharType> & src, std::basic_string<CharType> & dest, UnicodeConversionContext & context) {
    return value_manip::from_driver<std::basic_string<DriverPivotNarrowCharType>>::template to_application<CharType*>::convert(src, dest, context);
}

template <typename CharType>
inline void fromUTF8(const std::basic_string_view<DriverPivotNarrowCharType> & src, std::basic_string<CharType> & dest) {
    UnicodeConversionContext context;
    return fromUTF8<CharType>(src, dest, context);
}

template <typename CharType>
inline void fromUTF8(const std::basic_string<DriverPivotNarrowCharType> & src, std::basic_string<CharType> & dest, UnicodeConversionContext & context) {
    return fromUTF8<CharType>(make_string_view(src), dest, context);
}

template <typename CharType>
inline void fromUTF8(const std::basic_string<DriverPivotNarrowCharType> & src, std::basic_string<CharType> & dest) {
    return fromUTF8<CharType>(make_string_view(src), dest);
}

template <typename CharType>
inline void fromUTF8(const DriverPivotNarrowCharType * src, SQLLEN src_length, std::basic_string<CharType> & dest, UnicodeConversionContext & context) {
    return value_manip::from_driver<std::basic_string<DriverPivotNarrowCharType>>::template to_application<CharType*>::convert(src, src_length, dest, context);
}

template <typename CharType>
inline void fromUTF8(const DriverPivotNarrowCharType * src, std::basic_string<CharType> & dest, UnicodeConversionContext & context) {
    return fromUTF8<CharType>(src, SQL_NTS, dest, context);
}

template <typename CharType>
inline void fromUTF8(const DriverPivotNarrowCharType * src, SQLLEN src_length, std::basic_string<CharType> & dest) {
    UnicodeConversionContext context;
    return fromUTF8<CharType>(src, src_length, dest, context);
}

template <typename CharType>
inline void fromUTF8(const DriverPivotNarrowCharType * src, std::basic_string<CharType> & dest) {
    return fromUTF8<CharType>(src, SQL_NTS, dest);
}

