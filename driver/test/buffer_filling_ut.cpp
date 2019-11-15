#include "driver/utils/utils.h"

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

        constexpr std::size_t padding_size = 10; // to detect possible out-of-bounds writes
        const std::size_t size = padding_size + std::max(buffer_size, data_size) + padding_size;

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
            ASSERT_LT(data_size, buffer_size);
        }
        catch (const SqlException& ex) {
            // With current configuration we only expect right truncations.
            ASSERT_EQ(ex.getSQLState(), "01004");
            ASSERT_EQ(ex.getReturnCode(), SQL_SUCCESS_WITH_INFO);
            ASSERT_GE(data_size, buffer_size);
        }

        if (check_with_length_in_bytes) {
            EXPECT_EQ(returned_data_size % sizeof(CharType), 0);
            returned_data_size /= sizeof(CharType);
        }

        EXPECT_EQ(returned_data_size, data_size);

        // For 'buffer_size == 0' the 'expected' is the same unchanged initial buffer.
        // For 'buffer_size > 0' we fill 'expected' with what we expect - the data filled into the sized buffer.
        if (buffer_size > 0) {
            const auto data_wstr = fromUTF8<CharType>(data_str);
            ASSERT_EQ(data_wstr.size(), data_str.size());

            auto result_size = data_wstr.size();
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
using Strings_##CharType = Strings<CharType>; \
                                              \
TEST_P(Strings_##CharType, Fill){             \
    fill(                                     \
        std::get<0>(GetParam()),              \
        std::get<1>(GetParam()),              \
        std::get<2>(GetParam())               \
    );                                        \
}                                             \
                                              \
INSTANTIATE_TEST_CASE_P(                      \
    BufferFilling,                            \
    Strings_##CharType,                       \
    ::testing::Combine(                       \
        ::testing::Range<std::size_t>(0, 10), /* buffer sizes                      */ \
        ::testing::Range<std::size_t>(0, 10), /* data sizes                        */ \
        ::testing::Values(true, false)        /* lengths in bytes or in characters */ \
    )                                         \
);

using signed_char = signed char;
using unsigned_char = unsigned char;
using unsigned_short = unsigned short;

DECLARE_TEST_GROUP(char);
DECLARE_TEST_GROUP(signed_char);
DECLARE_TEST_GROUP(unsigned_char);
DECLARE_TEST_GROUP(char16_t);
DECLARE_TEST_GROUP(char32_t);
DECLARE_TEST_GROUP(wchar_t);
DECLARE_TEST_GROUP(unsigned_short);

#undef DECLARE_TEST_GROUP
