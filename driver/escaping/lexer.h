#pragma once

#include "string_view.h"

#include <deque>
#include <vector>

// Allow same declaration as in lexer.cpp
#define DECLARE(NAME) NAME
#define DECLARE_SQL_TSI(NAME) SQL_TSI_##NAME

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

        DECLARE(ABS),
        DECLARE(ACOS),
        DECLARE(ASIN),
        DECLARE(ATAN),
        // DECLARE(ATAN2),
        DECLARE(CEILING),
        DECLARE(COS),
        // DECLARE(COT),
        // DECLARE(DEGREES),
        DECLARE(EXP),
        DECLARE(FLOOR),
        DECLARE(LOG),
        DECLARE(LOG10),
        DECLARE(MOD),
        DECLARE(PI),
        DECLARE(POWER),
        // DECLARE(RADIANS),
        DECLARE(RAND),
        DECLARE(ROUND),
        // DECLARE(SIGN),
        DECLARE(SIN),
        DECLARE(SQRT),
        DECLARE(TAN),
        DECLARE(TRUNCATE),

        DECLARE(CONCAT),
        DECLARE(CONVERT),
        DECLARE(TIMESTAMPDIFF),
        DECLARE(TIMESTAMPADD),
        DECLARE(CURDATE),
        DECLARE(CURRENT_TIMESTAMP),
        DECLARE(CURRENT_DATE),
        DECLARE(DAYOFWEEK),
        DECLARE(DAYOFYEAR),
        DECLARE(LOCATE),
        DECLARE(LCASE),
        DECLARE(LTRIM),
        DECLARE(REPLACE),

        DECLARE(EXTRACT),

        // for TIMESTAMPDIFF
        //DECLARE(SQL_TSI_FRAC_SECOND),
        DECLARE(SQL_TSI_SECOND),
        DECLARE(SQL_TSI_MINUTE),
        DECLARE(SQL_TSI_HOUR),
        DECLARE(SQL_TSI_DAY),
        DECLARE(SQL_TSI_WEEK),
        DECLARE(SQL_TSI_MONTH),
        DECLARE(SQL_TSI_QUARTER),
        DECLARE(SQL_TSI_YEAR),

        // Delimiters
        COMMA,   //  ,
        LPARENT, //  (
        RPARENT, //  )
        LCURLY,  //  {
        RCURLY,  //  }
    };

#undef DECLARE
#undef DECLARE_SQL_TSI

    Type type;
    StringView literal;

    Token() : type(INVALID) {}

    Token(Type t, StringView l) : type(t), literal(l) {}

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
    const char * cur_;
    const char * end_;
    /// Recognized tokens.
    std::deque<Token> readed_;
    bool emit_space_;
};

/// Convers all letters to upper-case.
std::string to_upper(const StringView & str);
