#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/utils.h"

#include <unicode/ustring.h>
#include <unicode/ucnv.h>

#include <string>
#include <string_view>
#include <vector>

using ConverterPivotWideCharType = UChar;
inline const std::string converter_pivot_wide_char_encoding = "UTF-16";

bool sameEncoding(const std::string & lhs, const std::string & rhs);

// Return the same string with signature prefix removed, if it matches the provided signature/BOM byte array.
// Even though the signature is specified in bytes, it will be considered matching only if it matches up to some character boundary.
template <typename CharType>
inline auto consumeSignature(std::basic_string_view<CharType> str, const std::string_view & signature) {
    if (
        signature.size() > 0 &&
        (signature.size() % sizeof(CharType)) == 0 &&
        (str.size() * sizeof(CharType)) >= signature.size() &&
        std::memcmp(&str[0], &signature[0], signature.size()) == 0
    ) {
        str.remove_prefix(signature.size() / sizeof(CharType));
    }

    return str;
}

template <typename CharType>
inline std::size_t consumeSignatureInPlace(std::basic_string<CharType> & str, const std::string_view & signature) {
    const auto str_v_no_sig = consumeSignature(make_string_view(str), signature);
    const auto symbols_to_trim = str.size() - str_v_no_sig.size();

    if (symbols_to_trim > 0) {
        // Doing this manually to guarantee that buffer pointers are not invalidated by the implementation.
        const auto new_size = str.size() - symbols_to_trim;
        std::memmove(&str[0], &str[symbols_to_trim], new_size * sizeof(CharType));
        str.resize(new_size);
    }

    return symbols_to_trim; // Already trimmed symbols count, actually.
}

class UnicodeConverter;

template <typename SourceCharType, typename DestinationCharType, typename PivotCharType>
inline void convertEncoding(
    UnicodeConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot,
    UnicodeConverter & dest_converter, std::basic_string<DestinationCharType> & dest,
    const bool ensure_src_signature = true,
    const bool trim_dest_signature = true
);

template <typename SourceCharType, typename DestinationCharType, typename PivotCharType>
inline void convertEncoding(
    UnicodeConverter & src_converter, const std::basic_string<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot,
    UnicodeConverter & dest_converter, std::basic_string<DestinationCharType> & dest,
    const bool ensure_src_signature = true,
    const bool trim_dest_signature = true
) {
    return convertEncoding(src_converter, make_string_view(src), pivot, dest_converter, dest, ensure_src_signature, trim_dest_signature);
}

// A wrapper class around an ICU converter. Converter is created by providing an encoding.
// Converter is able to convert between that encoding and ICU's hardcoded pivot encoding.
// Pivot string is always stored in UChar * buffers. Encoded string is stored in buffers of
// getEncodedMinCharSize()-byte sized characters (even though pointers to such buffers are
// passed around as char * internally.)
class UnicodeConverter {
public:
    explicit UnicodeConverter(const std::string & encoding);
    ~UnicodeConverter();

    inline const std::size_t getEncodedMinCharSize() const;

    template <typename CharType>
    inline std::basic_string_view<CharType> consumeEncodedSignature(const std::basic_string_view<CharType> & str) const;

    template <typename CharType>
    inline std::basic_string_view<CharType> consumePivotSignature(const std::basic_string_view<CharType> & str) const;

    template <typename CharType>
    inline std::size_t consumeEncodedSignatureInPlace(std::basic_string<CharType> & str) const;

    template <typename CharType>
    inline std::size_t consumePivotSignatureInPlace(std::basic_string<CharType> & str) const;

    template <typename CharType, typename PivotCharType>
    inline void convertToPivot(
        const std::basic_string_view<CharType> & encoded,
        std::basic_string<PivotCharType> & pivot,
        const bool ensure_encoded_signature,
        const bool trim_pivot_signature
    );

    template <typename CharType, typename PivotCharType>
    inline void convertFromPivot(
        const std::basic_string_view<PivotCharType> & pivot,
        std::basic_string<CharType> & encoded,
        const bool ensure_pivot_signature,
        const bool trim_encoded_signature
    );

    template <typename SourceCharType, typename DestinationCharType, typename PivotCharType>
    friend inline void convertEncoding(
        UnicodeConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
        std::basic_string<PivotCharType> & pivot,
        UnicodeConverter & dest_converter, std::basic_string<DestinationCharType> & dest,
        const bool ensure_src_signature,
        const bool trim_dest_signature
    );

private:
    UConverter * converter_ = nullptr;

    std::string encoded_signature_to_prepend_;
    std::vector<std::string> encoded_signatures_to_trim_;
    std::size_t encoded_signatures_to_trim_max_size_ = 0;

    std::string pivot_signature_to_prepend_;
    std::vector<std::string> pivot_signatures_to_trim_;
    std::size_t pivot_signatures_to_trim_max_size_ = 0;
};

inline const std::size_t UnicodeConverter::getEncodedMinCharSize() const {
    return ucnv_getMinCharSize(converter_);
}

template <typename CharType>
inline std::basic_string_view<CharType> UnicodeConverter::consumeEncodedSignature(const std::basic_string_view<CharType> & str) const {
    if (!str.empty()) {
        for (auto & signature : encoded_signatures_to_trim_) {
            auto str_no_sig = ::consumeSignature(str, make_string_view(signature));
            if (str_no_sig.size() < str.size())
                return str_no_sig;
        }
    }

    return str;
}

template <typename CharType>
inline std::basic_string_view<CharType> UnicodeConverter::consumePivotSignature(const std::basic_string_view<CharType> & str) const {
    if (!str.empty()) {
        for (auto & signature : pivot_signatures_to_trim_) {
            auto str_no_sig = ::consumeSignature(str, make_string_view(signature));
            if (str_no_sig.size() < str.size())
                return str_no_sig;
        }
    }

    return str;
}

template <typename CharType>
inline std::size_t UnicodeConverter::consumeEncodedSignatureInPlace(std::basic_string<CharType> & str) const {
    if (!str.empty()) {
        for (auto & signature : encoded_signatures_to_trim_) {
            const auto symbols_consumed = ::consumeSignatureInPlace(str, make_string_view(signature));
            if (symbols_consumed > 0)
                return symbols_consumed;
        }
    }

    return 0;
}

template <typename CharType>
inline std::size_t UnicodeConverter::consumePivotSignatureInPlace(std::basic_string<CharType> & str) const {
    if (!str.empty()) {
        for (auto & signature : pivot_signatures_to_trim_) {
            const auto symbols_consumed = ::consumeSignatureInPlace(str, make_string_view(signature));
            if (symbols_consumed > 0)
                return symbols_consumed;
        }
    }

    return 0;
}

template <typename CharType, typename PivotCharType>
inline void UnicodeConverter::convertToPivot(
    const std::basic_string_view<CharType> & encoded,
    std::basic_string<PivotCharType> & pivot,
    const bool ensure_encoded_signature,
    const bool trim_pivot_signature
) {
    pivot.clear();

    static_assert(sizeof(PivotCharType) == sizeof(ConverterPivotWideCharType), "unable to convert encoding: unsuitable character type for the pivot");

    if (sizeof(CharType) != getEncodedMinCharSize())
        throw std::runtime_error("unable to convert encoding: unsuitable character type for the source converter");

    try {
        constexpr std::size_t extra_size_reserve = 32;
        constexpr std::size_t min_size_increment = 128;
        constexpr std::size_t conservative_target_write_increment = 16;

        const auto initial_new_size = std::min(encoded.size() + extra_size_reserve, min_size_increment);
        resize_without_initialization(pivot, initial_new_size);

        auto * source = reinterpret_cast<const char *>(encoded.data());
        auto * source_end = reinterpret_cast<const char *>(encoded.data() + encoded.size());

        auto * target = const_cast<ConverterPivotWideCharType *>(reinterpret_cast<const ConverterPivotWideCharType *>(pivot.c_str()));
        auto * target_end = reinterpret_cast<const ConverterPivotWideCharType *>(pivot.c_str() + pivot.size());

        std::size_t target_symbols_written = 0;
        bool pivot_signature_trimmed = false;

        // If signature must be prepended to the encoded string before decoding, we feed it to ucnv_toUnicode() separately, to avoid heavy copying.
        if (ensure_encoded_signature) {
            auto encoded_no_sig = consumeEncodedSignature(encoded);
            if (encoded_no_sig.size() == encoded.size()) {
                auto * target_prev = target;
                auto * sig_source = reinterpret_cast<const char *>(encoded_signature_to_prepend_.c_str());
                auto * sig_source_end = reinterpret_cast<const char *>(encoded_signature_to_prepend_.c_str() + encoded_signature_to_prepend_.size());
                UErrorCode error_code = U_ZERO_ERROR;

                ucnv_toUnicode(converter_, &target, target_end, &sig_source, sig_source_end, nullptr, false, &error_code);

                target_symbols_written += target - target_prev;

                // Assuming, target had enough space to hold any signature.

                if (U_FAILURE(error_code)) {
                    throw std::runtime_error(u_errorName(error_code));
                }
                else if (sig_source != sig_source_end) {
                    throw std::runtime_error("unable to convert encoding: failed to fully decode prepended signature");
                }
            }
        }

        // Main streaming conversion loop.
        while (true) {
            auto * target_prev = target;
            const auto * conservative_target_end_candidate = target + conservative_target_write_increment;
            const auto * conservative_target_end = (conservative_target_end_candidate < target_end ? conservative_target_end_candidate : target_end);
            const auto * final_target_end = (trim_pivot_signature && !pivot_signature_trimmed ? conservative_target_end : target_end);
            UErrorCode error_code = U_ZERO_ERROR;

            ucnv_toUnicode(converter_, &target, final_target_end, &source, source_end, nullptr, true, &error_code);

            target_symbols_written += target - target_prev;

            while (
                trim_pivot_signature &&
                !pivot_signature_trimmed &&
                (target_symbols_written * sizeof(ConverterPivotWideCharType)) >= pivot_signatures_to_trim_max_size_
            ) {
                pivot.resize(target_symbols_written); // Shrink to avoid memmove'ing unused bytes.
                const auto consumed_symbols = consumePivotSignatureInPlace(pivot); // pivot is not shorter than pivot_signatures_to_trim_max_size_, so even 0 is valid.
                target_symbols_written -= consumed_symbols;
                target -= consumed_symbols;
                pivot_signature_trimmed = (consumed_symbols == 0); // This means we may try several times, and stop only when all possible duplicate leading signatures are consumed.
            }

            if (error_code == U_BUFFER_OVERFLOW_ERROR) {
                const std::size_t source_pending_bytes = source_end - source;
                const std::size_t source_pending_symbols = (source_pending_bytes / sizeof(CharType)) + (source_pending_bytes % sizeof(CharType) > 0 ? 1 : 0);
                const std::size_t target_new_size = target_symbols_written + source_pending_symbols;
                const std::size_t final_new_size = std::max(target_new_size + extra_size_reserve, pivot.size() + min_size_increment);

                resize_without_initialization(pivot, final_new_size);

                target = const_cast<ConverterPivotWideCharType *>(reinterpret_cast<const ConverterPivotWideCharType *>(pivot.c_str())) + target_symbols_written;
                target_end = reinterpret_cast<const ConverterPivotWideCharType *>(pivot.c_str() + pivot.size());
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
        ucnv_resetToUnicode(converter_);
        pivot.clear();
        throw;
    }
}

template <typename CharType, typename PivotCharType>
inline void UnicodeConverter::convertFromPivot(
    const std::basic_string_view<PivotCharType> & pivot,
    std::basic_string<CharType> & encoded,
    const bool ensure_pivot_signature,
    const bool trim_encoded_signature
) {
    encoded.clear();

    static_assert(sizeof(PivotCharType) == sizeof(ConverterPivotWideCharType), "unable to convert encoding: unsuitable character type for the pivot");

    if (sizeof(CharType) != getEncodedMinCharSize())
        throw std::runtime_error("unable to convert encoding: unsuitable character type for the destination converter");

    try {
        constexpr std::size_t extra_size_reserve = 32;
        constexpr std::size_t min_size_increment = 128;
        constexpr std::size_t conservative_target_write_increment = 16;

        const auto initial_new_size = std::min(pivot.size() + extra_size_reserve, min_size_increment);
        resize_without_initialization(encoded, initial_new_size);

        auto * source = reinterpret_cast<const ConverterPivotWideCharType *>(pivot.data());
        auto * source_end = reinterpret_cast<const ConverterPivotWideCharType *>(pivot.data() + pivot.size());

        auto * target = const_cast<char *>(reinterpret_cast<const char *>(encoded.c_str()));
        auto * target_end = reinterpret_cast<const char *>(encoded.c_str() + encoded.size());

        std::size_t target_symbols_written = 0;
        bool encoded_signature_trimmed = false;

        // If signature must be prepended to the pivot before encoding, we feed it to ucnv_fromUnicode() separately, to avoid heavy copying.
        if (ensure_pivot_signature) {
            auto pivot_no_sig = consumePivotSignature(pivot);
            if (pivot_no_sig.size() == pivot.size()) {
                auto * target_prev = target;
                auto * sig_source = reinterpret_cast<const ConverterPivotWideCharType *>(pivot_signature_to_prepend_.c_str());
                auto * sig_source_end = reinterpret_cast<const ConverterPivotWideCharType *>(pivot_signature_to_prepend_.c_str() + pivot_signature_to_prepend_.size());
                UErrorCode error_code = U_ZERO_ERROR;

                ucnv_fromUnicode(converter_, &target, target_end, &sig_source, sig_source_end, nullptr, false, &error_code);

                const auto target_bytes_appended = target - target_prev;

                if (target_bytes_appended % sizeof(CharType) != 0)
                    throw std::runtime_error("unable to convert encoding: ucnv_fromUnicode() failed to write destination buffer to symbol boundary");

                target_symbols_written += target_bytes_appended / sizeof(CharType);

                // Assuming, target had enough space to hold any signature.

                if (U_FAILURE(error_code)) {
                    throw std::runtime_error(u_errorName(error_code));
                }
                else if (sig_source != sig_source_end) {
                    throw std::runtime_error("unable to convert encoding: failed to fully encode prepended signature");
                }
            }
        }

        // Main streaming conversion loop.
        while (true) {
            auto * target_prev = target;
            const auto * conservative_target_end_candidate = target + conservative_target_write_increment * sizeof(CharType);
            const auto * conservative_target_end = (conservative_target_end_candidate < target_end ? conservative_target_end_candidate : target_end);
            const auto * final_target_end = (trim_encoded_signature && !encoded_signature_trimmed ? conservative_target_end : target_end);
            UErrorCode error_code = U_ZERO_ERROR;

            ucnv_fromUnicode(converter_, &target, final_target_end, &source, source_end, nullptr, true, &error_code);

            const auto target_bytes_appended = target - target_prev;

            if (target_bytes_appended % sizeof(CharType) != 0)
                throw std::runtime_error("unable to convert encoding: ucnv_fromUnicode() failed to write destination buffer to symbol boundary");

            target_symbols_written += target_bytes_appended / sizeof(CharType);

            while (
                trim_encoded_signature &&
                !encoded_signature_trimmed &&
                (target_symbols_written * sizeof(CharType)) >= encoded_signatures_to_trim_max_size_
            ) {
                encoded.resize(target_symbols_written); // Shrink to avoid memmove'ing unused bytes.
                const auto consumed_symbols = consumeEncodedSignatureInPlace(encoded); // encoded is not shorter than encoded_signatures_to_trim_max_size_, so even 0 is valid.
                target_symbols_written -= consumed_symbols;
                target -= consumed_symbols * sizeof(CharType);
                encoded_signature_trimmed = (consumed_symbols == 0); // This means we may try several times, and stop only when all possible duplicate leading signatures are consumed.
            }

            if (error_code == U_BUFFER_OVERFLOW_ERROR) {
                const std::size_t source_pending_symbols = source_end - source;
                const std::size_t target_new_size = target_symbols_written + source_pending_symbols;
                const std::size_t final_new_size = std::max(target_new_size + extra_size_reserve, encoded.size() + min_size_increment);

                resize_without_initialization(encoded, final_new_size);

                target = const_cast<char *>(reinterpret_cast<const char *>(encoded.c_str() + target_symbols_written));
                target_end = reinterpret_cast<const char *>(encoded.c_str() + encoded.size());
            }
            else if (U_FAILURE(error_code)) {
                throw std::runtime_error(u_errorName(error_code));
            }
            else {
                encoded.resize(target_symbols_written);
                break;
            }
        }
    }
    catch (...) {
        ucnv_resetFromUnicode(converter_);
        encoded.clear();
        throw;
    }
}

template <typename SourceCharType, typename DestinationCharType, typename PivotCharType>
inline void convertEncoding(
    UnicodeConverter & src_converter, const std::basic_string_view<SourceCharType> & src,
    std::basic_string<PivotCharType> & pivot,
    UnicodeConverter & dest_converter, std::basic_string<DestinationCharType> & dest,
    const bool ensure_src_signature,
    const bool trim_dest_signature
) {
#if defined(WORKAROUND_ICU_USE_EXPLICIT_PIVOTING)
    src_converter.convertToPivot(src, pivot, ensure_src_signature, false);
    dest_converter.convertFromPivot(make_string_view(pivot), dest, true, trim_dest_signature);
#else
    dest.clear();

    static_assert(sizeof(PivotCharType) == sizeof(ConverterPivotWideCharType), "unable to convert encoding: unsuitable character type for the pivot");

    if (sizeof(SourceCharType) != src_converter.getEncodedMinCharSize())
        throw std::runtime_error("unable to convert encoding: unsuitable character type for the source converter");

    if (sizeof(DestinationCharType) != dest_converter.getEncodedMinCharSize())
        throw std::runtime_error("unable to convert encoding: unsuitable character type for the destination converter");

    try {
        constexpr std::size_t extra_size_reserve = 32;
        constexpr std::size_t min_size_increment = 128;
        constexpr std::size_t conservative_target_write_increment = 16;

        const auto initial_new_size = std::min(src.size() + extra_size_reserve, min_size_increment);
        resize_without_initialization(dest, initial_new_size);

        constexpr std::size_t pivot_size = 1024;
        resize_without_initialization(pivot, pivot_size);

        auto * source = reinterpret_cast<const char *>(src.data());
        auto * source_end = reinterpret_cast<const char *>(src.data() + src.size());

        auto * target = const_cast<char *>(reinterpret_cast<const char *>(dest.c_str()));
        auto * target_end = reinterpret_cast<const char *>(dest.c_str() + dest.size());

        auto * pivot_start = const_cast<ConverterPivotWideCharType *>(reinterpret_cast<const ConverterPivotWideCharType *>(pivot.data()));
        auto * pivot_source = pivot_start;
        auto * pivot_target = pivot_start;
        auto * pivot_end = reinterpret_cast<const ConverterPivotWideCharType *>(pivot.data() + pivot.size());

        std::size_t target_symbols_written = 0;
        bool dest_signature_trimmed = false;

        // If signature must be prepended to the encoded string before decoding, we feed it to ucnv_convertEx() separately, to avoid heavy copying.
        if (ensure_src_signature) {
            auto src_no_sig = src_converter.consumeEncodedSignature(src);
            if (src_no_sig.size() == src.size()) {
                auto * target_prev = target;
                auto * sig_source = reinterpret_cast<const char *>(src_converter.encoded_signature_to_prepend_.c_str());
                auto * sig_source_end = reinterpret_cast<const char *>(src_converter.encoded_signature_to_prepend_.c_str() + src_converter.encoded_signature_to_prepend_.size());
                UErrorCode error_code = U_ZERO_ERROR;

                ucnv_convertEx(dest_converter.converter_, src_converter.converter_, &target, target_end, &sig_source, sig_source_end, pivot_start, &pivot_source, &pivot_target, pivot_end, false, false, &error_code);

                const auto target_bytes_appended = target - target_prev;

                if (target_bytes_appended % sizeof(DestinationCharType) != 0)
                    throw std::runtime_error("unable to convert encoding: ucnv_convertEx() failed to write destination buffer to symbol boundary");

                target_symbols_written += target_bytes_appended / sizeof(DestinationCharType);

                // Assuming, target had enough space to hold any signature.

                if (U_FAILURE(error_code)) {
                    throw std::runtime_error(u_errorName(error_code));
                }
                else if (sig_source != sig_source_end) {
                    throw std::runtime_error("unable to convert encoding: failed to fully decode prepended signature");
                }
            }
        }

        // Main streaming conversion loop.
        while (true) {
            auto * target_prev = target;
            const auto * conservative_target_end_candidate = target + conservative_target_write_increment * sizeof(DestinationCharType);
            const auto * conservative_target_end = (conservative_target_end_candidate < target_end ? conservative_target_end_candidate : target_end);
            const auto * final_target_end = (trim_dest_signature && !dest_signature_trimmed ? conservative_target_end : target_end);
            UErrorCode error_code = U_ZERO_ERROR;

            ucnv_convertEx(dest_converter.converter_, src_converter.converter_, &target, final_target_end, &source, source_end, pivot_start, &pivot_source, &pivot_target, pivot_end, false, true, &error_code);

            const auto target_bytes_appended = target - target_prev;

            if (target_bytes_appended % sizeof(DestinationCharType) != 0)
                throw std::runtime_error("unable to convert encoding: ucnv_convertEx() failed to write destination buffer to symbol boundary");

            target_symbols_written += target_bytes_appended / sizeof(DestinationCharType);

            while (
                trim_dest_signature &&
                !dest_signature_trimmed &&
                (target_symbols_written * sizeof(DestinationCharType)) >= dest_converter.encoded_signatures_to_trim_max_size_
            ) {
                dest.resize(target_symbols_written); // Shrink to avoid memmove'ing unused bytes.
                const auto consumed_symbols = dest_converter.consumeEncodedSignatureInPlace(dest); // dest is not shorter than dest_converter.encoded_signatures_to_trim_max_size_, so even 0 is valid.
                target_symbols_written -= consumed_symbols;
                target -= consumed_symbols * sizeof(DestinationCharType);
                dest_signature_trimmed = (consumed_symbols == 0); // This means we may try several times, and stop only when all possible duplicate leading signatures are consumed.
            }

            if (error_code == U_BUFFER_OVERFLOW_ERROR) {
                const std::size_t source_pending_symbols = (source_end - source) / sizeof(SourceCharType) + ((source_end - source) % sizeof(SourceCharType) == 0 ? 0 : 1);
                const std::size_t target_new_size = target_symbols_written + source_pending_symbols;
                const std::size_t final_new_size = std::max(target_new_size + extra_size_reserve, dest.size() + min_size_increment);

                resize_without_initialization(dest, final_new_size);

                target = const_cast<char *>(reinterpret_cast<const char *>(dest.c_str() + target_symbols_written));
                target_end = reinterpret_cast<const char *>(dest.c_str() + dest.size());
            }
            else if (U_FAILURE(error_code)) {
                throw std::runtime_error(u_errorName(error_code));
            }
            else {
                dest.resize(target_symbols_written);
                break;
            }
        }
    }
    catch (...) {
        ucnv_resetToUnicode(src_converter.converter_);
        ucnv_resetFromUnicode(dest_converter.converter_);
        dest.clear();
        throw;
    }
#endif
}
