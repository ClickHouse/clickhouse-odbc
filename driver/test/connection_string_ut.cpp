#include "driver/config/config.h"

#include <gtest/gtest.h>

class ParseToMap
    : public ::testing::TestWithParam<std::tuple<std::string, key_value_map_t>>
{
protected:
    inline void parse_and_compare(const std::string & cs, const key_value_map_t & cs_map_expected) {
        const auto cs_map = readConnectionString(cs);

        EXPECT_EQ(cs_map.size(), cs_map_expected.size());

        for (const auto & pair : cs_map) {
            const auto & key = pair.first;
            const auto & value = pair.second;

            const auto it = cs_map_expected.find(key);
            ASSERT_NE(it, cs_map_expected.end());

            const auto & expected_value = it->second;
            EXPECT_EQ(value, expected_value);
        }
    }
};

TEST_P(ParseToMap, Compare) { parse_and_compare(std::get<0>(GetParam()), std::get<1>(GetParam())); }

INSTANTIATE_TEST_CASE_P(ConnectionString, ParseToMap,
    ::testing::ValuesIn(std::initializer_list<std::tuple<std::string, key_value_map_t>>{
        { "", { } },
        { " ", { } },
        { "   ", { } },
        { ";", { } },
        { ";;;", { } },
        { " ; ", { } },
        { ";  ", { } },
        { "  ;", { } },
        { "  ; ; ;", { } },
        { "x=", {
            { "x", "" }
        } },
        { "x=y", {
            { "x", "y" }
        } },
        { " ;  x  =  y  ; ", {
            { "x", "y" }
        } },
        { "x=;y=;z=", {
            { "x", "" }, { "y", "" }, { "z", "" }
        } },
        { "key1=value1; key2 = value 2 ; key 3 = {; v{a= lu ;e3 = }", {
            { "key1", "value1" },
            { "key2", "value 2" },
            { "key 3", "; v{a= lu ;e3 = " }
        } },
        { "key1=value1; key2 = value 2 ; key1 = value3;", {
            { "key1", "value1" },
            { "key2", "value 2" }
        } },
        { "key==1=value; ==key2 = value ; key1== = value ;== ===value", {
            { "key=1", "value" },
            { "=key2", "value" },
            { "key1=", "value" },
            { "= =", "value" }
        } },
    })
);
