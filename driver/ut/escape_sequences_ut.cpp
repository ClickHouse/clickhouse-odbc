#include <escaping/escape_sequences.h>
#include <escaping/lexer.h>
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
        "SELECT concat('a','b')"
    );

    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONCAT(`table`.`field1`, `table`.`field1`)}"),
        "SELECT concat(`table`.`field1`,`table`.`field1`)"
    );

    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONCAT({fn CONCAT(`table`.`field1`, '.')}, `table`.`field1`)}"),
        "SELECT concat(concat(`table`.`field1`,'.'),`table`.`field1`)"
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

TEST(LexerCase, ParseString) {
    Token tok = Lexer("'2017-01-01'").Consume();

    ASSERT_EQ(tok.type, Token::STRING);
    ASSERT_EQ(tok.literal, "'2017-01-01'");
}
