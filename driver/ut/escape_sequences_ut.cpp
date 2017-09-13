#include <escaping/escape_sequences.h>
#include <gtest/gtest.h>

TEST(EscapeSequencesCase, ParseConvert) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONVERT(1, SQL_BIGINT)}"),
        "SELECT toInt64(1)"
    );
}

TEST(EscapeSequencesCase, ParseConcat) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONCAT('a', 'b')}"),
        "SELECT concat('a', 'b')"
    );

    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONCAT(`table`.`field1`, `table`.`field1`)}"),
        "SELECT concat(`table`.`field1`, `table`.`field1`)"
    );

    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONCAT({fn CONCAT(`table`.`field1`, '.')}, `table`.`field1`)}"),
        "SELECT concat(concat(`table`.`field1`, '.'), `table`.`field1`)"
    );
}

TEST(EscapeSequencesCase, ParseRound) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn ROUND(1.1 + 2.4, 1)}"),
        "SELECT round(1.1 + 2.4, 1)"
    );
}

TEST(EscapeSequencesCase, DateTime) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {d '2017-01-01'}"),
        "SELECT toDate('2017-01-01')"
    );

    ASSERT_EQ(
        replaceEscapeSequences("SELECT {ts '2017-01-01 10:01:01'}"),
        "SELECT toDateTime('2017-01-01 10:01:01')"
    );
}
