#pragma once

#include "string_view.h"

#include <deque>
#include <vector>

struct Token {
    enum Type {
        INVALID = 0,
        EOS,

        // Identifiers and literals
        IDENT,
        NUMBER,

        // Keywords
        FN,
        CONVERT,

        // Delimiters
        COMMA,      //  ,
        LPARENT,    //  (
        RPARENT,    //  )
        LCURLY,     //  {
        RCURLY,     //  }
    };

    Type type;
    StringView literal;

    Token()
        : type(INVALID)
    { }

    Token(Type t, StringView l)
        : type(t)
        , literal(l)
    { }
};

class Lexer {
public:
    explicit Lexer(const StringView text);

    /// Returns next token from input stream.
    Token Consume();

    /// Returns next token if its type is equal to expected or error otherwise.
    Token Consume(Token::Type expected);

    /// Checks whether type of next token is equal to expected.
    /// Skips token if true.
    bool Match(Token::Type expected);

private:
    /// Makes token of length len againts current position.
    Token MakeToken(const Token::Type type, size_t len);

    /// Recoginze next token.
    Token NextToken();

private:
    const StringView text_;
    /// Pointer to current char in the input string.
    const char* cur_;
    const char* end_;
    /// Recognized tokens.
    std::deque<Token> readed_;
};

/// Convers all letters to upper-case.
std::string to_upper(const StringView& str);
