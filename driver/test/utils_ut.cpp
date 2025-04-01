#include "driver/utils/sql_encoding.h"
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

INSTANTIATE_TEST_SUITE_P(CatalogFnVLArgs, ParseToSet,
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

TEST(Escaping, ToSqlQuery)
{
    ASSERT_EQ(toSqlQueryValue(std::string("")), "''");
    ASSERT_EQ(toSqlQueryValue(std::string("foo")), "'foo'");
    ASSERT_EQ(toSqlQueryValue(std::string("foo'bar")), "'foo\\'bar'");
    ASSERT_EQ(toSqlQueryValue(std::optional<std::string>{"foo'bar"}), "'foo\\'bar'");
    ASSERT_EQ(toSqlQueryValue(bool{true}), "1");
    ASSERT_EQ(toSqlQueryValue(bool{false}), "0");
    ASSERT_EQ(toSqlQueryValue(int8_t{-42}), "-42");
    ASSERT_EQ(toSqlQueryValue(uint8_t{42}), "42");
    ASSERT_EQ(toSqlQueryValue(int16_t{-42}), "-42");
    ASSERT_EQ(toSqlQueryValue(uint16_t{42}), "42");
    ASSERT_EQ(toSqlQueryValue(int32_t{-42}), "-42");
    ASSERT_EQ(toSqlQueryValue(uint32_t{42}), "42");
    ASSERT_EQ(toSqlQueryValue(int64_t{-42}), "-42");
    ASSERT_EQ(toSqlQueryValue(uint64_t{42}), "42");
    ASSERT_EQ(toSqlQueryValue(std::optional<std::string>{}), "NULL");
    ASSERT_EQ(toSqlQueryValue(std::optional<int8_t>{}), "NULL");
    ASSERT_EQ(toSqlQueryValue(std::optional<uint8_t>{}), "NULL");
    ASSERT_EQ(toSqlQueryValue(std::optional<int16_t>{}), "NULL");
    ASSERT_EQ(toSqlQueryValue(std::optional<uint16_t>{}), "NULL");
    ASSERT_EQ(toSqlQueryValue(std::optional<int32_t>{}), "NULL");
    ASSERT_EQ(toSqlQueryValue(std::optional<uint32_t>{}), "NULL");
    ASSERT_EQ(toSqlQueryValue(std::optional<int64_t>{}), "NULL");
    ASSERT_EQ(toSqlQueryValue(std::optional<uint64_t>{}), "NULL");
}
