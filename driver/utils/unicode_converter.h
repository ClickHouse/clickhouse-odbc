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

template <typename CharType>
inline auto make_raw_str(std::initializer_list<CharType> && list) {
    return std::string{list.begin(), list.end()};
}

inline auto sameEncoding(const std::string & lhs, const std::string & rhs) {
    return (ucnv_compareNames(lhs.c_str(), rhs.c_str()) == 0);
}

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

class UnicodeConverter {
public:
    explicit inline UnicodeConverter(const std::string & encoding);
    inline ~UnicodeConverter();

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

private:
    UConverter * converter_ = nullptr;

    std::string encoded_signature_to_prepend_;
    std::vector<std::string> encoded_signatures_to_trim_;
    std::size_t encoded_signatures_to_trim_max_size_ = 0;

    std::string pivot_signature_to_prepend_;
    std::vector<std::string> pivot_signatures_to_trim_;
    std::size_t pivot_signatures_to_trim_max_size_ = 0;
};

inline UnicodeConverter::UnicodeConverter(const std::string & encoding) {
    // Create ICU converter instance.
    {
        UErrorCode error_code = U_ZERO_ERROR;
        UConverter * converter = ucnv_open(encoding.c_str(), &error_code);

        if (U_FAILURE(error_code))
            throw std::runtime_error(u_errorName(error_code));

        if (!converter)
            throw std::runtime_error("ucnv_open(" + encoding + ") failed");

        converter_ = converter;
    }

    // Fill/detect some signature/BOM info.
    {
        // For those encodings for which force_non_empty_signature_to_prepend_detection is false,
        // encoded_signature_to_prepend_ will be chosen only if the converter generates one itself.
        bool force_non_empty_signature_to_prepend_detection = true;

        // Treating plain UTF-16/UTF-32 as if they are in the native byte-order.
        if (sameEncoding(encoding, "UTF-1")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0xF7, 0x64, 0x4C }));
        }
        else if (sameEncoding(encoding, "UTF-7")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0x2B, 0x2F, 0x76, 0x38, 0x2D })); // ...this should come first.
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0x2B, 0x2F, 0x76, 0x38 }));
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0x2B, 0x2F, 0x76, 0x39 }));
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0x2B, 0x2F, 0x76, 0x2B }));
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0x2B, 0x2F, 0x76, 0x2F }));
        }
        else if (sameEncoding(encoding, "UTF-8")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0xEF, 0xBB, 0xBF }));
        }
        else if (sameEncoding(encoding, "UTF-EBCDIC")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0xDD, 0x73, 0x66, 0x73 }));
        }
        else if (
            sameEncoding(encoding, "UTF-16BE") || (sameEncoding(encoding, "UTF-16") && !isLittleEndian()) ||
            sameEncoding(encoding, "UCS-2BE") || (sameEncoding(encoding, "UCS-2") && !isLittleEndian())
        ) {
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0xFE, 0xFF }));
        }
        else if (
            sameEncoding(encoding, "UTF-16LE") || (sameEncoding(encoding, "UTF-16") && isLittleEndian()) ||
            sameEncoding(encoding, "UCS-2LE") || (sameEncoding(encoding, "UCS-2") && isLittleEndian())
        ) {
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0xFF, 0xFE }));
        }
        else if (
            sameEncoding(encoding, "UTF-32BE") || (sameEncoding(encoding, "UTF-32") && !isLittleEndian()) ||
            sameEncoding(encoding, "UCS-4BE") || (sameEncoding(encoding, "UCS-4") && !isLittleEndian())
        ) {
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0x00, 0x00, 0xFE, 0xFF }));
        }
        else if (
            sameEncoding(encoding, "UTF-32LE") || (sameEncoding(encoding, "UTF-32") && isLittleEndian()) ||
            sameEncoding(encoding, "UCS-4LE") || (sameEncoding(encoding, "UCS-4") && isLittleEndian())
        ) {
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0xFF, 0xFE, 0x00, 0x00 }));
        }
        else if (sameEncoding(encoding, "SCSU")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0x0E, 0xFE, 0xFF }));
        }
        else if (sameEncoding(ecnoding, "BOCU-1")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0xFB, 0xEE, 0x28 }));
        }
        else if (sameEncoding(ecnoding, "GB-18030")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoded_signatures_to_trim_.push_back(make_raw_str({ 0x84, 0x31, 0x95, 33 }));
        }

        // Detect the signature generated by the converter. Inspired by the code of ICU's uconv utility.

        std::basic_string<UChar> pivot;
        pivot.push_back(UChar(0xFEFF)); // Pivot's signature/BOM.
        pivot.push_back(UChar(0x61)); // 'a'

        auto detect_signature = [&] (auto char_tag) {
            using DestinationCharType = std::decay_t<decltype(char_tag)>;

            std::basic_string<DestinationCharType> dest;
            dest.resize(128); // This should be big enought to store the biggest possible signature + encoded 'a' of any known encoding.

            {
                UErrorCode error_code = U_ZERO_ERROR;

                auto * source = pivot.c_str();
                auto * source_end = pivot.c_str() + pivot.size();

                auto * target = const_cast<char *>(reinterpret_cast<const char *>(dest.c_str()));
                auto * target_end = reinterpret_cast<const char *>(dest.c_str() + dest.size());
                auto * target_prev = target;

                ucnv_fromUnicode(converter_, &target, target_end, &source, source_end, nullptr, true, &error_code);

                if (
                    U_FAILURE(error_code) ||
                    source != source_end ||
                    target_end < target ||
                    ((target - target_prev) % sizeof(DestinationCharType)) != 0
                ) {
                    throw std::runtime_error("unable to detect signature: helper ucnv_fromUnicode() failed");
                }

                dest.resize((target - target_prev) / sizeof(DestinationCharType));
            }

            {
                UErrorCode error_code = U_ZERO_ERROR;
                std::int32_t signature_length = 0; // in bytes

                const auto * charset_name = ucnv_detectUnicodeSignature(reinterpret_cast<const char *>(dest.c_str()), dest.size() * sizeof(DestinationCharType), &signature_length, &error_code);

                if (
                    U_SUCCESS(error_code) &&
                    charset_name != nullptr &&
                    signature_length > 0 &&
                    signature_length < (dest.size() * sizeof(DestinationCharType)) &&
                    signature_length % sizeof(DestinationCharType) == 0
                ) {
                    dest.resize(signature_length / sizeof(DestinationCharType));
                }
                else {
                    dest.clear();
                }
            }

            encoded_signature_to_prepend_.assign(reinterpret_cast<const char *>(dest.c_str()), dest.size() * sizeof(DestinationCharType));
        };

        switch (getEncodedMinCharSize()) {
            case 1: detect_signature(char{});     break;
            case 2: detect_signature(char16_t{}); break;
            case 4: detect_signature(char32_t{}); break;
            default: throw std::runtime_error("unable to detect signature: unable to choose a character type for the converter");
        }

        if (!encoded_signature_to_prepend_.empty()) {
            auto it = std::find(encoded_signatures_to_trim_.begin(), encoded_signatures_to_trim_.end(), encoded_signature_to_prepend_);
            if (it == encoded_signatures_to_trim_.end())
                encoded_signatures_to_trim_.push_back(encoded_signature_to_prepend_);
        }
        else if (
            force_non_empty_signature_to_prepend_detection &&
            !encoded_signatures_to_trim_.empty()
        ) {
            encoded_signature_to_prepend_ = encoded_signatures_to_trim_.front();
        }

        for (auto & signature : encoded_signatures_to_trim_) {
            if (signature.size() > encoded_signatures_to_trim_max_size_)
                encoded_signatures_to_trim_max_size_ = signature.size();
        }

        // Pivot is hardcoded UTF-16, see converter_pivot_wide_char_encoding.
        pivot_signature_to_prepend_ = (isLittleEndian() ? make_raw_str({ 0xFF, 0xFE }) : make_raw_str({ 0xFE, 0xFF }));
        pivot_signatures_to_trim_.push_back(pivot_signature_to_prepend_);
        pivot_signatures_to_trim_max_size_ = pivot_signature_to_prepend_.size();
    }
}

inline UnicodeConverter::~UnicodeConverter() {
    if (converter_) {
        ucnv_close(converter_);
        converter_ = nullptr;
    }
}

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

            if (
                trim_pivot_signature &&
                !pivot_signature_trimmed &&
                (target_symbols_written * sizeof(ConverterPivotWideCharType)) >= pivot_signatures_to_trim_max_size_
            ) {
                pivot.resize(target_symbols_written); // Shrink to avoid memmove'ing unused bytes.
                const auto consumed_symbols = consumePivotSignatureInPlace(pivot); // pivot is not shorter than pivot_signatures_to_trim_max_size_, so even 0 is valid.
                target_symbols_written -= consumed_symbols;
                target -= consumed_symbols;
                pivot_signature_trimmed = true;
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

                target_symbols_written += target - target_prev;

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

            if (
                trim_encoded_signature &&
                !encoded_signature_trimmed &&
                (target_symbols_written * sizeof(CharType)) >= encoded_signatures_to_trim_max_size_
            ) {
                encoded.resize(target_symbols_written); // Shrink to avoid memmove'ing unused bytes.
                const auto consumed_symbols = consumeEncodedSignatureInPlace(encoded); // encoded is not shorter than encoded_signatures_to_trim_max_size_, so even 0 is valid.
                target_symbols_written -= consumed_symbols;
                target -= consumed_symbols * sizeof(CharType);
                encoded_signature_trimmed = true;
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
