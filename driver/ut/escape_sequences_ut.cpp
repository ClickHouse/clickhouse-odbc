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

TEST(EscapeSequencesCase, ParsePower) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn POWER(`f_g38d`.`hsf_thkd_wect_fxge`,2)}"),
        "SELECT pow(`f_g38d`.`hsf_thkd_wect_fxge`,2)"
    );
}

TEST(EscapeSequencesCase, ParseTruncate) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT CAST({fn TRUNCATE(1.1 + 2.4, 1)} AS INTEGER) AS `yr_date_ok`"),
              "SELECT CAST(trunc(1.1 + 2.4, 1) AS INTEGER) AS `yr_date_ok`"
    );
    //ASSERT_EQ(
    //    replaceEscapeSequences("SELECT CAST({fn TRUNCATE(EXTRACT(YEAR FROM `Custom_SQL_Query`.`date`),0)} AS INTEGER) AS `yr_date_ok`"),
    //          "TODO: convert extract() function"
    //);
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

    // We cutting off milliseconds from timestamp because CH server
    // doesn't support them.
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {ts '2017-01-01 10:01:01.555'}"),
        "SELECT toDateTime('2017-01-01 10:01:01')"
    );
    // Strange date format. Symbols after last dot shouldn't be cutted off.
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {ts '2017.01.01 10:01:01'}"),
        "SELECT toDateTime('2017.01.01 10:01:01')"
    );
}
