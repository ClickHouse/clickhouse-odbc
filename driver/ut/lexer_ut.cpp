#include <escaping/lexer.h>
#include <gtest/gtest.h>

TEST(LexerCase, ParseString) {
    Token tok = Lexer("'2017-01-01'").Consume();

    ASSERT_EQ(tok.type, Token::STRING);
    ASSERT_EQ(tok.literal, "'2017-01-01'");

}

TEST(LexerCase, ParseIdent) {
    auto tok = Lexer("nonquoted").Consume();
    ASSERT_EQ(tok.type, Token::IDENT);
    tok = Lexer("nonquoted_string").Consume();
    ASSERT_EQ(tok.type, Token::IDENT);
    tok = Lexer("table.field").Consume();
    ASSERT_EQ(tok.type, Token::IDENT);
    tok = Lexer("`table`.`field`").Consume();
    ASSERT_STREQ(tok.literal.to_string().c_str(), "`table`.`field`");
    ASSERT_EQ(tok.type, Token::IDENT);
    tok = Lexer("Custom_SQL_Query.amount").Consume();
    ASSERT_STREQ(tok.literal.to_string().c_str(), "Custom_SQL_Query.amount");
}
