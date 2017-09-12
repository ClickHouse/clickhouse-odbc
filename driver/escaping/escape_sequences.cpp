#include "escape_sequences.h"
#include "lexer.h"

using namespace std;

namespace {

string convertFunctionByType(const std::string& typeName) {
    if (typeName == "SQL_BIGINT") {
        return "toInt64";
    }
    if (typeName == "SQL_INTEGER") {
        return "toInt32";
    }
    return string();
}

string processFunction(const StringView seq, Lexer& lex) {
    if (!lex.Match(Token::CONVERT)) {
        return seq.to_string();
    }
    if (!lex.Match(Token::LPARENT)) {
        return seq.to_string();
    }

    Token num = lex.Consume();
    if (num.type != Token::NUMBER) {
        return seq.to_string();
    }
    if (!lex.Match(Token::COMMA)) {
        return seq.to_string();
    }
    Token type = lex.Consume();
    if (type.type != Token::IDENT) {
        return seq.to_string();
    }

    string func = convertFunctionByType(type.literal.to_string());

    if (!func.empty()) {
        return func + "(" + num.literal.to_string() + ")";
    }

    return seq.to_string();
}

string processDate(const StringView seq, Lexer& lex) {
    Token data = lex.Consume(Token::STRING);
    if (data.isInvalid()) {
        return seq.to_string();
    } else {
        return string("toDate('") + data.literal.to_string() + "')";
    }
}

string processDateTime(const StringView seq, Lexer& lex) {
    Token data = lex.Consume(Token::STRING);
    if (data.isInvalid()) {
        return seq.to_string();
    } else {
        return string("toDateTime('") + data.literal.to_string() + "')";
    }
}

string processEscapeSequences(const StringView seq) {
    Lexer lex(seq);

    Token cmd = lex.Consume();
    switch (cmd.type) {
        case Token::FN:
            return processFunction(seq, lex);
        case Token::D:
            return processDate(seq, lex);
        case Token::TS:
            return processDateTime(seq, lex);

        // Unimplemented
        case Token::T:
        default:
            break;
    }

    return seq.to_string();
}

} // namespace

std::string replaceEscapeSequences(const std::string & query)
{
    const char* p = query.c_str();
    const char* end = p + query.size();
    const char* st = p;
    int level = 0;
    std::string ret;

    while (p != end) {
        switch (*p) {
            case '{': // TODO {fn
                if (level == 0) {
                    if (st < p) {
                        ret += std::string(st, p);
                    }
                    st = p + 1;
                }
                level++;
                break;

            case '}':
                if (level == 0) {
                    // TODO unexpected '}'
                    return query;
                }
                if (--level == 0) {
                    ret += processEscapeSequences(StringView(st, p));
                    st = p + 1;
                }
                break;
        }

        ++p;
    }

    if (st < p) {
        ret += std::string(st, p);
    }

    return ret;
}
