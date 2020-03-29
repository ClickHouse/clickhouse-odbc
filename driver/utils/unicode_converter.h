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

// Return the same string but without provided leading signature/BOM bytes, if any.
template <typename CharType>
inline auto consumeSignature(std::basic_string_view<CharType> str, const std::string_view & signature) {
    if (
        signature.size() > 0 &&
        (str.size() * sizeof(CharType)) >= signature.size() &&
        std::memcmp(&str[0], &signature[0], signature.size()) == 0
    ) {
        str.remove_prefix(signature.size() / sizeof(CharType));
    }

    return str;
}

class UnicodeConverter {
private:
    inline static const std::string pivot_encoding_ = converter_pivot_wide_char_encoding;
    inline static const std::string pivot_signature_ = (isLittleEndian() ? make_raw_str({ 0xFF, 0xFE }) : make_raw_str({ 0xFE, 0xFF }));

public:
    explicit inline UnicodeConverter(const std::string & ecnoding);
    inline ~UnicodeConverter();

    inline const bool isNoOp() const;
    inline const std::size_t getEncodedMinCharSize() const;

    template <typename CharType>
    inline std::basic_string_view<CharType> consumeSignature(const std::basic_string_view<CharType> & str) const;

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

    bool is_noop_ = false;
    std::size_t encoded_min_char_size_ = 0;

    std::string encoding_signature_to_prepend_;
    std::vector<std::string> encoding_signatures_to_trim_;
};

inline UnicodeConverter::UnicodeConverter(const std::string & ecnoding) {
    // Create ICU converter instance.
    {
        UErrorCode error_code = U_ZERO_ERROR;
        UConverter * converter = ucnv_open(ecnoding.c_str(), &error_code);

        if (U_FAILURE(error_code))
            throw std::runtime_error(u_errorName(error_code));

        if (!converter)
            throw std::runtime_error("ucnv_open(" + ecnoding + ") failed");

        converter_ = converter;
    }

    // Read and cache some properties of the converter/encoding.
    {
        UErrorCode error_code = U_ZERO_ERROR;
        auto * converter_name = ucnv_getName(converter_, &error_code);

        if (U_FAILURE(error_code))
            throw std::runtime_error(u_errorName(error_code));

        if (!converter_name)
            throw std::runtime_error("ucnv_getName() failed");

        is_noop_ = sameEncoding(converter_name, pivot_encoding_);
        encoded_min_char_size_ = ucnv_getMinCharSize(converter_);
    }

    // Fill/detect some signature/BOM info.
    {
        // For those encodings for which force_non_empty_signature_to_prepend_detection is false,
        // encoding_signature_to_prepend_ will be chosen only if the converter generates one itself.
        bool force_non_empty_signature_to_prepend_detection = true;

        // Treating plain UTF-16/UTF-32 as if they are in the native byte-order.
        if (sameEncoding(ecnoding, "UTF-1")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0xF7, 0x64, 0x4C }));
        }
        else if (sameEncoding(ecnoding, "UTF-7")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0x2B, 0x2F, 0x76, 0x38, 0x2D })); // ...this should come first.
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0x2B, 0x2F, 0x76, 0x38 }));
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0x2B, 0x2F, 0x76, 0x39 }));
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0x2B, 0x2F, 0x76, 0x2B }));
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0x2B, 0x2F, 0x76, 0x2F }));
        }
        else if (sameEncoding(ecnoding, "UTF-8")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0xEF, 0xBB, 0xBF }));
        }
        else if (sameEncoding(ecnoding, "UTF-EBCDIC")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0xDD, 0x73, 0x66, 0x73 }));
        }
        else if (sameEncoding(ecnoding, "UTF-16BE") || (sameEncoding(ecnoding, "UTF-16") && !isLittleEndian())) {
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0xFE, 0xFF }));
        }
        else if (sameEncoding(ecnoding, "UTF-16LE") || (sameEncoding(ecnoding, "UTF-16") && isLittleEndian())) {
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0xFF, 0xFE }));
        }
        else if (sameEncoding(ecnoding, "UTF-32BE") || (sameEncoding(ecnoding, "UTF-32") && !isLittleEndian())) {
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0x00, 0x00, 0xFE, 0xFF }));
        }
        else if (sameEncoding(ecnoding, "UTF-32LE") || (sameEncoding(ecnoding, "UTF-32") && isLittleEndian())) {
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0xFF, 0xFE, 0x00, 0x00 }));
        }
        else if (sameEncoding(ecnoding, "SCSU")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0x0E, 0xFE, 0xFF }));
        }
        else if (sameEncoding(ecnoding, "BOCU-1")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0xFB, 0xEE, 0x28 }));
        }
        else if (sameEncoding(ecnoding, "GB-18030")) {
            force_non_empty_signature_to_prepend_detection = false;
            encoding_signatures_to_trim_.push_back(make_raw_str({ 0x84, 0x31, 0x95, 33 }));
        }

        // Detect the signature generated by the converter. Inspired by the code of ICU's uconv utility.

        std::basic_string<UChar> pivot;
        pivot.push_back(UChar(0x61)); // 'a'

        auto detect_signature = [&] (auto char_tag) {
            using DestinationCharType = std::decay_t<decltype(char_tag)>;

            std::basic_string<DestinationCharType> dest;
            dest.resize(1024); // Should be enough for any encoding to place its signature + encoded 'a' here.

            {
                UErrorCode error_code = U_ZERO_ERROR;

                auto * source = pivot.c_str();
                auto * source_end = pivot.c_str() + pivot.size();

                auto * target = const_cast<char *>(reinterpret_cast<const char *>(dest.c_str()));
                auto * target_end = reinterpret_cast<const char *>(dest.c_str() + dest.size());

                ucnv_fromUnicode(converter_, &target, target_end, &source, source_end, nullptr, true, &error_code);

                if (
                    U_FAILURE(error_code) ||
                    source != source_end ||
                    target_end < target ||
                    (target_end - target) % sizeof(DestinationCharType) != 0
                ) {
                    throw std::runtime_error("unable to detect signature: helper ucnv_fromUnicode() failed");
                }

                dest.resize((target_end - target) % sizeof(DestinationCharType));
            }

            {
                UErrorCode error_code = U_ZERO_ERROR;
                std::int32_t signature_length = 0;

                const auto * charset_name = ucnv_detectUnicodeSignature(reinterpret_cast<const char *>(dest.c_str()), dest.size() * sizeof(DestinationCharType), &signature_length, &error_code);

                if (
                    U_SUCCESS(error_code) &&
                    charset_name != nullptr &&
                    signature_length > 0 &&
                    signature_length < dest.size()
                ) {
                    dest.resize(signature_length);
                }
                else {
                    dest.clear();
                }
            }

            encoding_signature_to_prepend_.assign(reinterpret_cast<const char *>(dest.c_str()), dest.size() * sizeof(DestinationCharType));
        };

        switch (encoded_min_char_size_) {
            case 1: detect_signature(char{});     break;
            case 2: detect_signature(char16_t{}); break;
            case 4: detect_signature(char32_t{}); break;
            default: throw std::runtime_error("unable to detect signature: unable to choose a character type for the converter");
        }

        if (!encoding_signature_to_prepend_.empty()) {
            auto it = std::find(encoding_signatures_to_trim_.begin(), encoding_signatures_to_trim_.end(), encoding_signature_to_prepend_);
            if (it == encoding_signatures_to_trim_.end())
                encoding_signatures_to_trim_.push_back(encoding_signature_to_prepend_);
        }
        else if (
            force_non_empty_signature_to_prepend_detection &&
            !encoding_signatures_to_trim_.empty()
        ) {
            encoding_signature_to_prepend_ = encoding_signatures_to_trim_.front();
        }
    }
}

inline UnicodeConverter::~UnicodeConverter() {
    if (converter_) {
        ucnv_close(converter_);
        converter_ = nullptr;
    }
}

inline const bool UnicodeConverter::isNoOp() const {
    return is_noop_;
}

inline const std::size_t UnicodeConverter::getEncodedMinCharSize() const {
    return encoded_min_char_size_;
}

template <typename CharType>
inline std::basic_string_view<CharType> UnicodeConverter::consumeSignature(const std::basic_string_view<CharType> & str) const {
    if (!str.empty()) {
        for (auto & signature : encoding_signatures_to_trim_) {
            auto str_no_sig = ::consumeSignature(str, make_string_view(signature));
            if (str_no_sig.size() < str.size())
                return str_no_sig;
        }
    }

    return str;
}

template <typename CharType, typename PivotCharType>
inline void UnicodeConverter::convertToPivot(
    const std::basic_string_view<CharType> & encoded,
    std::basic_string<PivotCharType> & pivot,
    const bool ensure_encoded_signature,
    const bool trim_pivot_signature
) {
}

template <typename CharType, typename PivotCharType>
inline void UnicodeConverter::convertFromPivot(
    const std::basic_string_view<PivotCharType> & pivot,
    std::basic_string<CharType> & encoded,
    const bool ensure_pivot_signature,
    const bool trim_encoded_signature
) {
}
