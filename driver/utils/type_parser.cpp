#include "type_parser.h"

#include <sstream>

template <typename T>
static inline T fromString(const std::string & s) {
    std::istringstream iss(s);
    T result;
    iss >> result;
    return result;
}
static TypeAst::Meta getTypeMeta(const std::string & name) {
    if (name == "Array") {
        return TypeAst::Array;
    }

    if (name == "Null") {
        return TypeAst::Null;
    }

    if (name == "Nullable") {
        return TypeAst::Nullable;
    }

    if (name == "Tuple") {
        return TypeAst::Tuple;
    }

    return TypeAst::Terminal;
}


TypeParser::TypeParser(const std::string & name) : cur_(name.data()), end_(name.data() + name.size()), type_(nullptr) {}

TypeParser::~TypeParser() = default;

bool TypeParser::parse(TypeAst * type) {
    type_ = type;
    open_elements_.push(type_);

    do {
        const Token & token = nextToken();

        switch (token.type) {
            case Token::Name:
                type_->meta = getTypeMeta(token.value);
                type_->name = token.value;
                break;
            case Token::Number:
                type_->meta = TypeAst::Number;
                type_->size = fromString<int>(token.value);
                break;
            case Token::LPar:
                type_->elements.emplace_back(TypeAst());
                open_elements_.push(type_);
                type_ = &type_->elements.back();
                break;
            case Token::RPar:
                type_ = open_elements_.top();
                open_elements_.pop();
                break;
            case Token::Comma:
                type_ = open_elements_.top();
                open_elements_.pop();
                type_->elements.emplace_back(TypeAst());
                open_elements_.push(type_);
                type_ = &type_->elements.back();
                break;
            case Token::EOS:
                return true;
            case Token::Invalid:
                return false;
        }
    } while (true);
}

TypeParser::Token TypeParser::nextToken() {
    for (; cur_ < end_; ++cur_) {
        switch (*cur_) {
            case ' ':
            case '\n':
            case '\t':
            case '\0':
                continue;

            case '(':
                return Token {Token::LPar, std::string(cur_++, 1)};
            case ')':
                return Token {Token::RPar, std::string(cur_++, 1)};
            case ',':
                return Token {Token::Comma, std::string(cur_++, 1)};

            default: {
                const char * st = cur_;

                if (isalpha(*cur_)) {
                    for (; cur_ < end_; ++cur_) {
                        if (!isalpha(*cur_) && !isdigit(*cur_)) {
                            break;
                        }
                    }

                    return Token {Token::Name, std::string(st, cur_)};
                }

                if (isdigit(*cur_)) {
                    for (; cur_ < end_; ++cur_) {
                        if (!isdigit(*cur_)) {
                            break;
                        }
                    }

                    return Token {Token::Number, std::string(st, cur_)};
                }

                return Token {Token::Invalid, std::string()};
            }
        }
    }

    return Token {Token::EOS, std::string()};
}
