#include "driver/utils/utils.h"
#include "driver/utils/type_info.h"
#include "driver/test/client_utils.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <tuple>

template <typename CharType>
class Strings
    : public ::testing::TestWithParam<std::tuple<std::size_t, std::size_t, bool>>
{
protected:
    // 'check_with_length_in_bytes' is for 'fillOutputString(...)' argument passing method only,
    // 'buffer_size' and 'data_size' are still in characters here.
    void fill(const std::size_t buffer_size, const std::size_t data_size, const bool check_with_length_in_bytes) {
        // Just some non-trivial pattern of bits (that will convert to themselves when doing 'fromUTF8<CharType>(data_str)'.
        const std::string data_str(data_size, static_cast<char>(0b01011010));

#if defined(WORKAROUND_USE_ICU)
        UnicodeConversionContext context;
        auto & converter = (sizeof(CharType) == 1 ? context.application_narrow_char_converter : context.application_wide_char_converter);
        const auto data_wstr = fromUTF8<CharType>(data_str, context);
        ASSERT_EQ(stringLength(data_wstr, converter, context), data_str.size());
#else
        const auto data_wstr = fromUTF8<CharType>(data_str);
        ASSERT_EQ(data_wstr.size(), data_str.size());
#endif

        const auto converted_data_size = data_wstr.size();

        constexpr std::size_t padding_size = 10; // to detect possible out-of-bounds writes
        const std::size_t size = padding_size + std::max(buffer_size, converted_data_size) + padding_size;

        std::vector<CharType> expected(size);
        std::vector<CharType> result(size);

        // Fill all bits with 1's.
        std::memset(&expected[0], 0xFF, size * sizeof(CharType));
        std::memset(&result[0], 0xFF, size * sizeof(CharType));

        std::int64_t returned_data_size = 0;

        try {
            const auto rc = fillOutputString<CharType>(
                data_str,
                (buffer_size > 0 ? &result[padding_size] : nullptr),
                (check_with_length_in_bytes ? (buffer_size * sizeof(CharType)) : buffer_size),
                &returned_data_size,
                check_with_length_in_bytes
            );

            // All other outcomes are communicated via exceptions.
            EXPECT_EQ(rc, SQL_SUCCESS);
            ASSERT_LT(converted_data_size, buffer_size);
        }
        catch (const SqlException& ex) {
            // With current configuration we only expect right truncations.
            ASSERT_EQ(ex.getSQLState(), "01004");
            ASSERT_EQ(ex.getReturnCode(), SQL_SUCCESS_WITH_INFO);
            ASSERT_GE(converted_data_size, buffer_size);
        }

        if (check_with_length_in_bytes) {
            EXPECT_EQ(returned_data_size % sizeof(CharType), 0);
            returned_data_size /= sizeof(CharType);
        }

        EXPECT_EQ(returned_data_size, converted_data_size);

        // For 'buffer_size == 0' the 'expected' is the same unchanged initial buffer.
        // For 'buffer_size > 0' we fill 'expected' with what we expect - the data filled into the sized buffer.
        if (buffer_size > 0) {
            auto result_size = converted_data_size;
            if (result_size >= buffer_size)
                result_size = (buffer_size == 0 ? 0 : (buffer_size - 1));

            std::copy(data_wstr.begin(), data_wstr.begin() + result_size, expected.begin() + padding_size);
            expected[padding_size + result_size] = CharType{};
        }

        for (std::size_t i = 0; i < size; ++i) {
            EXPECT_EQ(result[i], expected[i]) << "Resulting and expected buffers differ at index " << i;
        }
    }
};

#define DECLARE_TEST_GROUP(CharType)          \
                                              \
using StringOf##CharType = Strings<CharType>; \
                                              \
TEST_P(StringOf##CharType, Fill){             \
    fill(                                     \
        std::get<0>(GetParam()),              \
        std::get<1>(GetParam()),              \
        std::get<2>(GetParam())               \
    );                                        \
}                                             \
                                              \
INSTANTIATE_TEST_SUITE_P(                     \
    BufferFilling,                            \
    StringOf##CharType,                       \
    ::testing::Combine(                       \
        ::testing::Range<std::size_t>(0, 10), /* buffer sizes                      */ \
        ::testing::Range<std::size_t>(0, 10), /* data sizes                        */ \
        ::testing::Values(true, false)        /* lengths in bytes or in characters */ \
    )                                         \
);

using SQLNarrowChar = char;
using SQLWideChar = char16_t;

DECLARE_TEST_GROUP(SQLNarrowChar);
DECLARE_TEST_GROUP(SQLWideChar);

#undef DECLARE_TEST_GROUP
