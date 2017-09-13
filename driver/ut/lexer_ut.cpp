#include <escaping/lexer.h>
#include <gtest/gtest.h>

TEST(LexerCase, ParseString) {
    Token tok = Lexer("'2017-01-01'").Consume();

    ASSERT_EQ(tok.type, Token::STRING);
    ASSERT_EQ(tok.literal, "'2017-01-01'");
}
