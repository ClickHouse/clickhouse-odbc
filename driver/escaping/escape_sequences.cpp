#include "escape_sequences.h"
#include "lexer.h"
#include <map>

using namespace std;

namespace {

const std::map<const std::string, const std::string> fn_convert_map {
    {"SQL_TINYINT", "toUInt8"},
    {"SQL_SMALLINT", "toUInt16"},
    {"SQL_INTEGER", "toInt32"},
    {"SQL_BIGINT",  "toInt64"},
    {"SQL_REAL", "toFloat32"},
    {"SQL_DOUBLE", "toFloat64"},
    {"SQL_VARCHAR", "toString"},
    {"SQL_DATE", "toDate"},
    {"SQL_TYPE_DATE", "toDate"},
    {"SQL_TIMESTAMP", "toDateTime"},
    {"SQL_TYPE_TIMESTAMP", "toDateTime"},
};

const std::map<const Token::Type, const std::string> function_map {
    {Token::ROUND,    "round" },
    {Token::POWER,    "pow"},
    {Token::TRUNCATE, "trunc"},
};

string processEscapeSequencesImpl(const StringView seq, Lexer& lex);

string convertFunctionByType(const StringView& typeName) {
    const auto type_name_string = typeName.to_string();
    if (fn_convert_map.find(type_name_string) != fn_convert_map.end())
        return fn_convert_map.at(type_name_string);

    return string();
}

string processFunction(const StringView seq, Lexer& lex) {
    const Token fn(lex.Consume());

    if (fn.type == Token::CONVERT) {
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
            if (!lex.Match(Token::RPARENT)) {
                return seq.to_string();
            }
            return func + "(" + num.literal.to_string() + ")";
        }
    } else if (function_map.find(fn.type) != function_map.end()) {
        string result = function_map.at(fn.type);
        lex.SetEmitSpaces(true);
        while (true) {
            const Token tok(lex.Peek());

            if (tok.type == Token::RCURLY) {
                break;
            } else if (tok.type == Token::LCURLY) {
                lex.SetEmitSpaces(false);
                result += processEscapeSequencesImpl(seq, lex);
                lex.SetEmitSpaces(true);
            } else if (tok.type == Token::EOS || tok.type == Token::INVALID) {
                break;
            } else {
                result += tok.literal.to_string();
                lex.Consume();
            }
        }
        lex.SetEmitSpaces(false);

        return result;
    } else if (fn.type == Token::CONCAT) {
        string result = "concat";

        lex.SetEmitSpaces(true);
        while (true) {
            const Token tok(lex.Peek());

            if (tok.type == Token::RCURLY) {
                break;
            } else if (tok.type == Token::LCURLY) {
                lex.SetEmitSpaces(false);
                result += processEscapeSequencesImpl(seq, lex);
                lex.SetEmitSpaces(true);
            } else if (tok.type == Token::EOS || tok.type == Token::INVALID) {
                break;
            } else {
                result += tok.literal.to_string();
                lex.Consume();
            }
        }
        lex.SetEmitSpaces(false);

        return result;
    }

    return seq.to_string();
}

string processDate(const StringView seq, Lexer& lex) {
    Token data = lex.Consume(Token::STRING);
    if (data.isInvalid()) {
        return seq.to_string();
    } else {
        return string("toDate(") + data.literal.to_string() + ")";
    }
}

string removeMilliseconds(const StringView token) {
    if (token.empty()) {
        return string();
    }

    const char* begin = token.data();
    const char* p = begin + token.size() - 1;
    const char* dot = nullptr;
    const bool quoted = (*p == '\'');
    if (quoted) {
        --p;
    }
    for (; p > begin; --p) {
        if (isdigit(*p)) {
            continue;
        }
        if (*p == '.') {
            if (dot) {
                return token.to_string();
            }
            dot = p;
        } else {
            if (dot) {
                return string(begin, dot) + (quoted ? "'" : "");
            }
            return token.to_string();
        }
    }

    return token.to_string();
}

string processDateTime(const StringView seq, Lexer& lex) {
    Token data = lex.Consume(Token::STRING);
    if (data.isInvalid()) {
        return seq.to_string();
    } else {
        return string("toDateTime(") + removeMilliseconds(data.literal) + ")";
    }
}

string processEscapeSequencesImpl(const StringView seq, Lexer& lex) {
    string result;

    if (!lex.Match(Token::LCURLY)) {
        return seq.to_string();
    }

    while (true) {
        const Token tok(lex.Consume());

        switch (tok.type) {
            case Token::FN:
                result += processFunction(seq, lex);
                break;

            case Token::D:
                result += processDate(seq, lex);
                break;
            case Token::TS:
                result += processDateTime(seq, lex);
                break;

            // End of escape sequence
            case Token::RCURLY:
                return result;

            // Unimplemented
            case Token::T:
            default:
                return seq.to_string();
        }
    };
}

string processEscapeSequences(const StringView seq) {
    Lexer lex(seq);
    return processEscapeSequencesImpl(seq, lex);
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
            case '{':
                if (level == 0) {
                    if (st < p) {
                        ret += std::string(st, p);
                    }
                    st = p;
                }
                level++;
                break;

            case '}':
                if (level == 0) {
                    // TODO unexpected '}'
                    return query;
                }
                if (--level == 0) {
                    ret += processEscapeSequences(StringView(st, p + 1));
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
