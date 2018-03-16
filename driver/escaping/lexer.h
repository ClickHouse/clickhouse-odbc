#pragma once

#include "string_view.h"

#include <deque>
#include <vector>

struct Token {
    enum Type {
        INVALID = 0,
        EOS,
        SPACE,
        OTHER,

        // Identifiers and literals
        IDENT,
        NUMBER,
        STRING,

        // Keywords
        FN,
        D,
        T,
        TS,
        CONCAT,
        CONVERT,
        ROUND,
        POWER,
        TRUNCATE,
        SQRT,
        ABS,
        TIMESTAMPDIFF,
        TIMESTAMPADD,
        CURRENT_TIMESTAMP,
        CURDATE,

        // for TIMESTAMPDIFF
        SQL_TSI_FRAC_SECOND,
        SQL_TSI_SECOND,
        SQL_TSI_MINUTE,
        SQL_TSI_HOUR,
        SQL_TSI_DAY,
        SQL_TSI_WEEK,
        SQL_TSI_MONTH,
        SQL_TSI_QUARTER,
        SQL_TSI_YEAR,

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

    inline bool isInvalid() const {
        return type == INVALID;
    }
};

class Lexer {
public:
    explicit Lexer(const StringView text);

    /// Returns next token from input stream.
    Token Consume();

    /// Returns next token if its type is equal to expected or error otherwise.
    Token Consume(Token::Type expected);

    /// Look at type of token at position n.
    Token LookAhead(size_t n);

    /// Checks whether type of next token is equal to expected.
    /// Skips token if true.
    bool Match(Token::Type expected);

    /// Peek next token.
    Token Peek();

    /// Enable or disable emitting of space tokens.
    void SetEmitSpaces(bool value);

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
    bool emit_space_;
};

/// Convers all letters to upper-case.
std::string to_upper(const StringView& str);
