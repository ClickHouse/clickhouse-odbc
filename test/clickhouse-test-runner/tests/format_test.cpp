#include "format.h"

#include <gtest/gtest.h>

using namespace runner;

TEST(Format, SplitOutputFormat) {
    struct Case {
        const char * in;
        const char * sql;
        const char * format;
        bool supported;
    };
    const std::vector<Case> cases = {
        {"SELECT 1", "SELECT 1", "TabSeparated", true},
        {"SELECT 1 FORMAT TSV", "SELECT 1 ", "TSV", true},
        {"SELECT 1 FORMAT TSVWithNames SETTINGS max_threads = 1", "SELECT 1  SETTINGS max_threads = 1", "TSVWithNames", true},
        {"SELECT 1 FORMAT JSONEachRow", "SELECT 1 ", "JSONEachRow", false},
        {"SELECT 'FORMAT JSON'", "SELECT 'FORMAT JSON'", "TabSeparated", true},
        {"SELECT * FROM (SELECT 1 FORMAT JSON)", "SELECT * FROM (SELECT 1 FORMAT JSON)", "TabSeparated", true},
        {"SELECT t.format FROM t", "SELECT t.format FROM t", "TabSeparated", true},
        {"SELECT 1 FORMAT null", "SELECT 1 ", "null", true},
        {"SELECT 1 FORMAT TSVRaw", "SELECT 1 ", "TSVRaw", true},
        {"SELECT 1 FORMAT `Null`;", "SELECT 1 ;", "Null", true},
        // FORMAT as an identifier, not the clause.
        {"SELECT format FROM t", "SELECT format FROM t", "TabSeparated", true},
        {"SELECT x AS format FROM t WHERE format = 1", "SELECT x AS format FROM t WHERE format = 1", "TabSeparated", true},
        {"SELECT 1 AS format FROM numbers(1)", "SELECT 1 AS format FROM numbers(1)", "TabSeparated", true},
        {"SELECT format FROM (SELECT 1 AS format)", "SELECT format FROM (SELECT 1 AS format)", "TabSeparated", true},
        {"SELECT 1 AS format", "SELECT 1 AS format", "TabSeparated", true},
        {"SELECT 1 AS format ORDER BY format DESC", "SELECT 1 AS format ORDER BY format DESC", "TabSeparated", true},
        {"SELECT 1 AS format ORDER BY format ASC", "SELECT 1 AS format ORDER BY format ASC", "TabSeparated", true},
        {"SELECT 1 AS format ORDER BY format DESC FORMAT TSV", "SELECT 1 AS format ORDER BY format DESC ", "TSV", true},
        // FORMAT after a keyword or an operator that needs an operand is a name: a table with an
        // alias, a column with an alias.
        {"SELECT * FROM format f", "SELECT * FROM format f", "TabSeparated", true},
        {"SELECT x FROM format f SETTINGS max_threads = 1", "SELECT x FROM format f SETTINGS max_threads = 1", "TabSeparated", true},
        {"SELECT format TSV", "SELECT format TSV", "TabSeparated", true},
        {"SELECT DISTINCT format TSV", "SELECT DISTINCT format TSV", "TabSeparated", true},
        {"SELECT 1 AS format, format TSV", "SELECT 1 AS format, format TSV", "TabSeparated", true},
        {"SELECT 2 * format TSV", "SELECT 2 * format TSV", "TabSeparated", true},
        {"SELECT * FROM format FORMAT TSV", "SELECT * FROM format ", "TSV", true},
        // An operand ends before FORMAT: a name that is also a keyword, a qualified name, the asterisk,
        // a literal, a cast, a query parameter, a keyword that ends a clause.
        {"SELECT 7 % 3 AS mod FORMAT TSV", "SELECT 7 % 3 AS mod ", "TSV", true},
        {"SELECT number AS offset FROM numbers(1) ORDER BY offset FORMAT TSV",
            "SELECT number AS offset FROM numbers(1) ORDER BY offset ",
            "TSV",
            true},
        {"SELECT t.limit FORMAT TSV", "SELECT t.limit ", "TSV", true},
        {"SHOW CREATE TABLE db.table FORMAT Null;", "SHOW CREATE TABLE db.table ;", "Null", true},
        {"SELECT * FORMAT TSV", "SELECT * ", "TSV", true},
        {"SELECT t.* FORMAT TSV", "SELECT t.* ", "TSV", true},
        {"SELECT x = 'a' FORMAT TSV", "SELECT x = 'a' ", "TSV", true},
        {"SELECT x = $$a$$ FORMAT TSV", "SELECT x = $$a$$ ", "TSV", true},
        {"SELECT -1 FORMAT TSV", "SELECT -1 ", "TSV", true},
        {"SELECT 1::Int8 FORMAT TSV", "SELECT 1::Int8 ", "TSV", true},
        {"SELECT {x:UInt8} FORMAT TSV", "SELECT {x:UInt8} ", "TSV", true},
        {"SELECT 1 GROUP BY ALL FORMAT TSV", "SELECT 1 GROUP BY ALL ", "TSV", true},
        {"SELECT 1 FROM t FINAL FORMAT TSV", "SELECT 1 FROM t FINAL ", "TSV", true},
        {"SELECT 1 LIMIT 10 FORMAT TSV", "SELECT 1 LIMIT 10 ", "TSV", true},
        {"SELECT CASE WHEN x THEN 1 ELSE 2 END FORMAT TSV", "SELECT CASE WHEN x THEN 1 ELSE 2 END ", "TSV", true},
        {"SELECT 1 FORMAT TSV -- c\n;", "SELECT 1  -- c\n;", "TSV", true},
        {"BACKUP TABLE t TO Memory('b') FORMAT Null;", "BACKUP TABLE t TO Memory('b') ;", "Null", true},
        {"KILL MUTATION WHERE mutation_id = 'm' SYNC FORMAT Null;", "KILL MUTATION WHERE mutation_id = 'm' SYNC ;", "Null", true},
    };
    for (const auto & c : cases) {
        std::string sql;
        const auto f = splitOutputFormat(c.in, sql);
        EXPECT_EQ(c.sql, sql) << c.in;
        EXPECT_EQ(c.format, f.name) << c.in;
        EXPECT_EQ(c.supported, f.supported) << c.in;
    }
}

TEST(Format, Flags) {
    std::string sql;
    auto f = splitOutputFormat("SELECT 1 FORMAT TabSeparatedRawWithNamesAndTypes", sql);
    EXPECT_TRUE(f.raw && f.names && f.types && !f.discard);
    f = splitOutputFormat("SELECT 1 FORMAT Null", sql);
    EXPECT_TRUE(f.discard);
}

TEST(Format, NullFormatResult) {
    EXPECT_TRUE(isNullFormatResult("HY000", "[HY000] Error while processing query 0f1e: 'Null' format is not supported"));
    EXPECT_TRUE(isNullFormatResult("HY000", "[HY000] Error while processing query 0f1e: 'NULL' format is not supported"));
    // Another format, another state, a server error that quotes the text, two records.
    EXPECT_FALSE(isNullFormatResult("HY000", "[HY000] Error while processing query 0f1e: 'JSON' format is not supported"));
    EXPECT_FALSE(isNullFormatResult("42000", "[42000] Error while processing query 0f1e: 'Null' format is not supported"));
    EXPECT_FALSE(isNullFormatResult("HY000",
        "[HY000] Error while processing query 0f1e: HTTP status code: 500\nReceived "
        "error:\nCode: 395. 'Null' format is not supported"));
    EXPECT_FALSE(isNullFormatResult("HY000", "[HY000] x: 'Null' format is not supported\n[01000] y"));
}

TEST(Format, ReturnsRows) {
    EXPECT_TRUE(returnsRows("SELECT 1"));
    EXPECT_TRUE(returnsRows("(SELECT 1)"));
    EXPECT_TRUE(returnsRows("with 1 as x select x"));
    EXPECT_TRUE(returnsRows("FROM t SELECT x"));
    EXPECT_TRUE(returnsRows("BACKUP TABLE t TO Memory('b') FORMAT Null"));
    EXPECT_TRUE(returnsRows("RESTORE TABLE t FROM Memory('b') FORMAT Null"));
    EXPECT_TRUE(returnsRows("-- c\nKILL MUTATION WHERE database = currentDatabase() FORMAT Null"));
    EXPECT_TRUE(returnsRows("kill query where query_id = 'q' sync format Null"));
    EXPECT_FALSE(returnsRows("INSERT INTO t SELECT 1 FORMAT TSV"));
    EXPECT_FALSE(returnsRows("CREATE TABLE t (x UInt8) ENGINE = Memory"));
}
