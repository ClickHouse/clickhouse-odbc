#include "driver/utils/utils.h"

#include <gtest/gtest.h>

using values_t = std::set<std::string>;

class ParseToSet
    : public ::testing::TestWithParam<std::tuple<std::string, values_t>>
{
protected:
    inline void parse_and_compare(const std::string & value_list, const values_t & values_expected) {
        const auto values = parseCatalogFnVLArgs(value_list);

        // TODO: enable once gtest is upgraded
//      EXPECT_THAT(values, ElementsAreArray(values_expected));

        // TODO: remove this once gtest is upgraded
        EXPECT_EQ(values.size(), values_expected.size());
        for (const auto & value : values) {
            EXPECT_NE(values_expected.find(value), values_expected.end());
        }
    }
};

TEST_P(ParseToSet, Compare) { parse_and_compare(std::get<0>(GetParam()), std::get<1>(GetParam())); }

INSTANTIATE_TEST_CASE_P(CatalogFnVLArgs, ParseToSet,
    ::testing::ValuesIn(std::initializer_list<std::tuple<std::string, values_t>>{
        { "", { "" } },
        { ",", { "" } },
        { " , '' , ' '", { "", " " } },
        { "TABLE,VIEW", { "TABLE", "VIEW" } },
        { "TABLE, VIEW", { "TABLE", "VIEW" } },
        { "'TABLE', 'VIEW'", { "TABLE", "VIEW" } },
        { "   TABLE   ,   VIEW   ", { "TABLE", "VIEW" } },
        { "   ,,   ,   VIEW   ", { "", "VIEW" } },
        { "'',TABLE,'VIEW'", { "", "TABLE", "VIEW" } },
        { ",TA\"BLE,', ,VIE,W, ,', ' , ,VIE,W, , ' ", { "", "TA\"BLE", ", ,VIE,W, ,", " , ,VIE,W, , " } }
    })
);

TEST(Escaping, EscapeForSQL) {
    for (const auto & pair : std::initializer_list<std::pair<std::string, std::string>>{
        { "", "" },
        { " ", " " },
        { "TABLE", "TABLE" },
        { "'", "\\'" },
        { "\\", "\\\\" },
        { " ' some \\x, \\' \\\\ value ' ", " \\' some \\\\x, \\\\\\' \\\\\\\\ value \\' " }
    }) {
        EXPECT_EQ(escapeForSQL(pair.first), pair.second);
    }
}

TEST(CatalogFnPattern, IsMatchAnything) {
    for (const auto & pair : std::initializer_list<std::pair<std::string, bool>>{
        { "", false },
        { " ", false },
        { "_", false },
        { " %", false },
        { "% ", false },
        { "%", true },
        { "%%%%%%", true },
        { "%_", false },
        { "_%", false },
        { "%_%", false },
        { " %%%%", false },
        { "%%%% ", false },
        { "A%", false },
        { "%A", false },
        { "%A%", false }
    }) {
        EXPECT_EQ(isMatchAnythingCatalogFnPatternArg(pair.first), pair.second);
    }
}
