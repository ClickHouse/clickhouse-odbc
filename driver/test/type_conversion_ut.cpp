#include "driver/utils/type_info.h"

#include <gtest/gtest.h>

#include <string>
#include <tuple>

class StringPong
    : public ::testing::Test
{
protected:
    template <typename T>
    inline void compare(const std::string & initial_str, const std::string & expected_str, bool case_sensitive = true) {
        T obj;
        value_manip::to_null(obj);
        value_manip::from_value<std::string>::template to_value<T>::convert(initial_str, obj);

        std::string resulting_str;
        value_manip::to_null(resulting_str);
        value_manip::from_value<T>::template to_value<std::string>::convert(obj, resulting_str);

        if (case_sensitive)
            ASSERT_STREQ(resulting_str.c_str(), expected_str.c_str());
        else
            ASSERT_STRCASEEQ(resulting_str.c_str(), expected_str.c_str());
    }
};

template <typename T>
class StringPongSymmetric
    : public StringPong
    , public ::testing::WithParamInterface<std::string>
{
protected:
    using DataType = T;
};

template <typename T>
class StringPongAsymmetric
    : public StringPong
    , public ::testing::WithParamInterface<std::tuple<std::string, std::string>>
{
protected:
    using DataType = T;
};

using StringPongGUIDSymmetric     = StringPongSymmetric<SQLGUID>;
using StringPongNumericSymmetric  = StringPongSymmetric<SQL_NUMERIC_STRUCT>;
using StringPongNumericAsymmetric = StringPongAsymmetric<SQL_NUMERIC_STRUCT>;

TEST_P(StringPongGUIDSymmetric,     Compare) { compare<DataType>(GetParam(), GetParam(), false/* case_sensitive */); }
TEST_P(StringPongNumericSymmetric,  Compare) { compare<DataType>(GetParam(), GetParam()); }
TEST_P(StringPongNumericAsymmetric, Compare) { compare<DataType>(std::get<0>(GetParam()), std::get<1>(GetParam())); }

INSTANTIATE_TEST_SUITE_P(TypeConversion, StringPongGUIDSymmetric,
    ::testing::Values(
        "00000000-0000-0000-0000-000000000000",
        "01020304-0506-0708-090A-0B0C0D0E0F00",
        "10203040-5060-7080-90A0-B0C0D0E0F000",
        "FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"
    )
);

INSTANTIATE_TEST_SUITE_P(TypeConversion, StringPongNumericSymmetric,
    ::testing::Values(
        "0",
        "12345",
        "-12345",
        "12345.6789",
        "-12345.6789",
        "12345.000000000000",
        "12345.001002003000",
        "10000000000000000000",
        "-10000000000000000000",
        ".00000000000000000001",
        "-.0000000000000000001",
        "9876543210987654321",
        ".9876543210987654321",
        "-9876543210987654321",
        "-.9876543210987654321",
        "9999999999999999999",
        "-9999999999999999999",
        ".9999999999999999999",
        "-.9999999999999999999",
        "18446744073709551615",
        "-18446744073709551615",
        ".18446744073709551615",
        "-.18446744073709551615"
    )
);

INSTANTIATE_TEST_SUITE_P(TypeConversion, StringPongNumericAsymmetric,
    ::testing::ValuesIn(std::initializer_list<std::tuple<std::string, std::string>>{
        { "0.", "0" },
        { "-0.", "0" },
        { "0.000", ".000" },
        { "-0.000", ".000" },
        { "0001.00001", "1.00001" },
        { "-0001.00001", "-1.00001" },
        { "000000.123", ".123" }
    })
);

using StringPongFloat32Symmetric = StringPongSymmetric<float>;
using StringPongFloat64Symmetric = StringPongSymmetric<double>;

TEST_P(StringPongFloat32Symmetric, Compare) { compare<DataType>(GetParam(), GetParam()); }
TEST_P(StringPongFloat64Symmetric, Compare) { compare<DataType>(GetParam(), GetParam()); }

INSTANTIATE_TEST_SUITE_P(TypeConversion, StringPongFloat32Symmetric,
    ::testing::Values(
        "0.1",
        "123.45679",
        "1e-07",
        "16777216",
        "3.4028235e+38"
    )
);

INSTANTIATE_TEST_SUITE_P(TypeConversion, StringPongFloat64Symmetric,
    ::testing::Values(
        "0",
        "-0",
        "42",
        "0.1",
        "123.456789123",
        "0.000001",
        "9.99e-07",
        "1e-07",
        "999000000000000000000",
        "1e+21",
        "1.7976931348623157e+308",
        "inf",
        "-inf"
    )
);

TEST(FloatToNumeric, Convert) {
    const auto convert = [] (auto value, SQLSCHAR scale) {
        SQL_NUMERIC_STRUCT numeric;
        value_manip::to_null(numeric);
        numeric.precision = 38;
        numeric.scale = scale;
        value_manip::from_value<decltype(value)>::template to_value<SQL_NUMERIC_STRUCT>::convert(value, numeric);

        std::string resulting_str;
        value_manip::to_null(resulting_str);
        value_manip::from_value<SQL_NUMERIC_STRUCT>::template to_value<std::string>::convert(numeric, resulting_str);
        return resulting_str;
    };

    EXPECT_EQ(convert(1e-7, 9), ".000000100");
    EXPECT_EQ(convert(1e-7f, 9), ".000000100");
    EXPECT_EQ(convert(1000000.0, 0), "1000000");
    EXPECT_EQ(convert(1e-300, 0), "0");
}
