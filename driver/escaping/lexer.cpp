#include "lexer.h"

#include <algorithm>
#include <unordered_map>

namespace {

static const std::unordered_map<std::string, Token::Type> KEYWORDS = {
    {"FN",      Token::FN},
    {"D",       Token::D},
    {"T",       Token::T},
    {"TS",      Token::TS},
    {"CONVERT", Token::CONVERT}
};

static Token::Type LookupIdent(const std::string& ident) {
    auto ki = KEYWORDS.find(ident);
    if (ki != KEYWORDS.end()) {
        return ki->second;
    }
    return Token::IDENT;
}

}

std::string to_upper(const StringView& str) {
    std::string ret(str.data(), str.size());
    std::transform(ret.begin(), ret.end(), ret.begin(), ::toupper);
    return ret;
}

Lexer::Lexer(const StringView text)
    : text_(text)
    , cur_(text.data())
    , end_(text.data() + text.size())
{
}

Token Lexer::Consume() {
    if (!readed_.empty()) {
        const Token token(readed_.front());
        readed_.pop_front();
        return token;
    }

    return NextToken();
}

Token Lexer::Consume(Token::Type expected) {
    if (readed_.empty()) {
        readed_.push_back(NextToken());
    }

    if (readed_.front().type == expected) {
        const Token token(readed_.front());
        readed_.pop_front();
        return token;
    }

    return Token{Token::INVALID, StringView()};
}

bool Lexer::Match(Token::Type expected) {
    if (readed_.empty()) {
        readed_.push_back(NextToken());
    }

    if (readed_.front().type != expected) {
        return false;
    }

    Consume();
    return true;
}


Token Lexer::MakeToken(const Token::Type type, size_t len) {
    const Token token{type, StringView(cur_, len)};

    for (; len > 0; --len) {
        ++cur_;
    }

    return token;
}

Token Lexer::NextToken() {
    for (; cur_ < end_; ++cur_) {
        switch (*cur_) {
            /** Whitespaces */

            case '\0':
            case ' ':
            case '\t':
            case '\f':
            case '\n':
            case '\r':
                continue;

            /** Delimiters */

            case '(':
                return MakeToken(Token::LPARENT, 1);
            case ')':
                return MakeToken(Token::RPARENT, 1);
            case '{':
                return MakeToken(Token::LCURLY, 1);
            case '}':
                return MakeToken(Token::RCURLY, 1);
            case ',':
                return MakeToken(Token::COMMA, 1);

            case '\'': {
                const char* st = ++cur_;
                bool has_slash = false;

                for (; cur_ < end_; ++cur_) {
                    if (*cur_ == '\\' && !has_slash) {
                        has_slash = true;
                        continue;
                    }
                    if (*cur_ == '\'' && !has_slash) {
                        return Token{
                            Token::STRING,
                            StringView(st, ++cur_ - st - 1)};
                    }

                    has_slash = false;
                }

                return Token{Token::INVALID, StringView(st, cur_ - st)};
            }

            default: {
                const char* st = cur_;

                if (isalpha(*cur_) || *cur_ == '_') {
                    for (++cur_; cur_ < end_; ++cur_) {
                        if (!isalpha(*cur_) && !isdigit(*cur_) && *cur_ != '_')
                        {
                            break;
                        }
                    }

                    return Token{
                        LookupIdent(to_upper(StringView(st, cur_))),
                        StringView(st, cur_)
                    };
                }

                if (isdigit(*cur_) || *cur_ == '.') {
                    bool has_dot = *cur_ == '.';

                    for (++cur_; cur_ < end_; ++cur_) {
                        if (*cur_ == '.') {
                            if (has_dot) {
                                return Token{Token::INVALID, StringView(st, cur_)};
                            } else {
                                has_dot = true;
                            }
                            continue;
                        }
                        if (!isdigit(*cur_)) {
                            break;
                        }
                    }

                    return Token{Token::NUMBER, StringView(st, cur_)};
                }

                return Token{Token::INVALID, StringView(st, cur_)};
            }
        }
    }

    return Token{Token::EOS, StringView()};
}
