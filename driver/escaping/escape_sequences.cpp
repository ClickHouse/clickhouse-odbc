/*
 
 https://docs.faircom.com/doc/sqlref/#33384.htm
 https://docs.microsoft.com/ru-ru/sql/odbc/reference/appendixes/time-date-and-interval-functions
 https://my.vertica.com/docs/7.2.x/HTML/index.htm#Authoring/SQLReferenceManual/Functions/Date-Time/TIMESTAMPADD.htm%3FTocPath%3DSQL%2520Reference%2520Manual%7CSQL%2520Functions%7CDate%252FTime%2520Functions%7C_____43

*/
#include "escape_sequences.h"

#include <iostream>
#include <map>
#include "lexer.h"
//#include "log.h"


using namespace std;

namespace {

const std::map<const std::string, const std::string> fn_convert_map{
    {"SQL_TINYINT", "toUInt8"},
    {"SQL_SMALLINT", "toUInt16"},
    {"SQL_INTEGER", "toInt32"},
    {"SQL_BIGINT", "toInt64"},
    {"SQL_REAL", "toFloat32"},
    {"SQL_DOUBLE", "toFloat64"},
    {"SQL_VARCHAR", "toString"},
    {"SQL_DATE", "toDate"},
    {"SQL_TYPE_DATE", "toDate"},
    {"SQL_TIMESTAMP", "toDateTime"},
    {"SQL_TYPE_TIMESTAMP", "toDateTime"},
};

#define DECLARE2(TOKEN, NAME) \
    { Token::TOKEN, NAME }

const std::map<const Token::Type, const std::string> function_map{
#include "function_declare.h"
};

#undef DECLARE

const std::map<const Token::Type, const std::string> function_map_strip_params{
    {Token::CURRENT_TIMESTAMP, "now()"},
};

const std::map<const Token::Type, const std::string> literal_map{
    // {Token::SQL_TSI_FRAC_SECOND, ""},
    {Token::SQL_TSI_SECOND, "'second'"},
    {Token::SQL_TSI_MINUTE, "'minute'"},
    {Token::SQL_TSI_HOUR, "'hour'"},
    {Token::SQL_TSI_DAY, "'day'"},
    {Token::SQL_TSI_WEEK, "'week'"},
    {Token::SQL_TSI_MONTH, "'month'"},
    {Token::SQL_TSI_QUARTER, "'quarter'"},
    {Token::SQL_TSI_YEAR, "'year'"},
};

const std::map<const Token::Type, const std::string> timeadd_func_map{
    // {Token::SQL_TSI_FRAC_SECOND, ""},
    {Token::SQL_TSI_SECOND, "addSeconds"},
    {Token::SQL_TSI_MINUTE, "addMinutes"},
    {Token::SQL_TSI_HOUR, "addHours"},
    {Token::SQL_TSI_DAY, "addDays"},
    {Token::SQL_TSI_WEEK, "addWeeks"},
    {Token::SQL_TSI_MONTH, "addMonths"},
    {Token::SQL_TSI_QUARTER, "addQuarters"},
    {Token::SQL_TSI_YEAR, "addYears"},
};


string processEscapeSequencesImpl(const StringView seq, Lexer & lex);

string convertFunctionByType(const StringView & typeName) {
    const auto type_name_string = typeName.to_string();
    if (fn_convert_map.find(type_name_string) != fn_convert_map.end())
        return fn_convert_map.at(type_name_string);

    return string();
}

string processParentheses(const StringView seq, Lexer & lex) {
    string result;
    lex.SetEmitSpaces(true);
    result += lex.Consume().literal.to_string(); // (

    while (true) {
        const Token token(lex.Peek());

        // std::cerr << __FILE__ << ":" << __LINE__ << " : "<< token.literal.to_string() << " type=" << token.type << " go\n";

        if (token.type == Token::RPARENT) {
            result += token.literal.to_string();
            lex.Consume();
            break;
        } else if (token.type == Token::LPARENT) {
            result += processParentheses(seq, lex);
        } else if (token.type == Token::LCURLY) {
            lex.SetEmitSpaces(false);
            result += processEscapeSequencesImpl(seq, lex);
            lex.SetEmitSpaces(true);
        } else if (token.type == Token::EOS || token.type == Token::INVALID) {
            break;
        } else {
            result += token.literal.to_string();
            lex.Consume();
        }
    }

    return result;
}

string processIdentOrFunction(const StringView seq, Lexer & lex) {
    while (lex.Match(Token::SPACE)) {
    }
    const auto token = lex.Peek();
    string result;

    if (token.type == Token::LCURLY) {
        lex.SetEmitSpaces(false);
        result += processEscapeSequencesImpl(seq, lex);
        lex.SetEmitSpaces(true);
    } else if (token.type == Token::LPARENT) {
        result += processParentheses(seq, lex);
    } else if (token.type == Token::IDENT && lex.LookAhead(1).type == Token::LPARENT) { // CAST( ... )
        result += token.literal.to_string();                                            // func name
        lex.Consume();
        result += processParentheses(seq, lex);
    } else if (token.type == Token::NUMBER || token.type == Token::IDENT || token.type == Token::STRING) {
        result += token.literal.to_string();
        lex.Consume();
    } else if (function_map_strip_params.find(token.type) != function_map_strip_params.end()) {
        result += function_map_strip_params.at(token.type);
    } else {
        return "";
    }
    while (lex.Match(Token::SPACE)) {
    }

    return result;
}

string processFunction(const StringView seq, Lexer & lex) {
    const Token fn(lex.Consume());

    if (fn.type == Token::CONVERT) {
        string result;
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();

        auto num = processIdentOrFunction(seq, lex);
        if (num.empty())
            return seq.to_string();
        result += num;

        while (lex.Match(Token::SPACE)) {
        }

        if (!lex.Match(Token::COMMA)) {
            return seq.to_string();
        }

        while (lex.Match(Token::SPACE)) {
        }

        Token type = lex.Consume();
        if (type.type != Token::IDENT) {
            return seq.to_string();
        }

        string func = convertFunctionByType(type.literal.to_string());

        if (!func.empty()) {
            while (lex.Match(Token::SPACE)) {
            }
            if (!lex.Match(Token::RPARENT)) {
                return seq.to_string();
            }
            result = func + "(" + result + ")";
        }

        return result;

    } else if (fn.type == Token::TIMESTAMPADD) {
        string result;
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();

        Token type = lex.Consume();
        if (timeadd_func_map.find(type.type) == timeadd_func_map.end())
            return seq.to_string();
        string func = timeadd_func_map.at(type.type);
        if (!lex.Match(Token::COMMA))
            return seq.to_string();
        auto ramount = processIdentOrFunction(seq, lex);
        if (ramount.empty())
            return seq.to_string();

        while (lex.Match(Token::SPACE)) {
        }

        if (!lex.Match(Token::COMMA))
            return seq.to_string();


        auto rdate = processIdentOrFunction(seq, lex);
        if (rdate.empty())
            return seq.to_string();

        if (!func.empty()) {
            while (lex.Match(Token::SPACE)) {
            }
            if (!lex.Match(Token::RPARENT)) {
                return seq.to_string();
            }
            result = func + "(" + rdate + ", " + ramount + ")";
        }
        return result;

    } else if (fn.type == Token::LOCATE) {
        string result;
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();

        auto needle = processIdentOrFunction(seq, lex /*, false */);
        if (needle.empty())
            return seq.to_string();
        lex.Consume();

        auto haystack = processIdentOrFunction(seq, lex /*, false*/);
        if (haystack.empty())
            return seq.to_string();
        lex.Consume();

        auto offset = processIdentOrFunction(seq, lex /*, false */);
        lex.Consume();

        result = "position(" + haystack + "," + needle + ")";

        return result;

    } else if (fn.type == Token::LTRIM) {
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();

        auto param = processIdentOrFunction(seq, lex /*, false*/);
        if (param.empty())
            return seq.to_string();
        lex.Consume();
        return "replaceRegexpOne(" + param + ", '^\\\\s+', '')";

    } else if (fn.type == Token::DAYOFWEEK) {
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();

        auto param = processIdentOrFunction(seq, lex /*, false*/);
        if (param.empty())
            return seq.to_string();
        lex.Consume();
        return "if(toDayOfWeek(" + param + ") = 7, 1, toDayOfWeek(" + param + ") + 1)";

    } else if (fn.type == Token::DAYOFYEAR) {
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();

        auto param = processIdentOrFunction(seq, lex /*, false*/);
        if (param.empty())
            return seq.to_string();
        lex.Consume();
        //return "if(toDayOfWeek(" + param + ") = 7, 1, toDayOfWeek(" + param + ") + 1)";
        return "( toRelativeDayNum(" + param + ") - toRelativeDayNum(toStartOfYear(" + param + ")) + 1 )";

    } else if (function_map.find(fn.type) != function_map.end()) {
        string result = function_map.at(fn.type);
        auto func = result;
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
            } else if (tok.type == Token::EXTRACT) {
                result += processFunction(seq, lex);
            } else {
                if (func != "EXTRACT" && literal_map.find(tok.type) != literal_map.end()) {
                    result += literal_map.at(tok.type);
                } else
                    result += tok.literal.to_string();
                lex.Consume();
            }
        }
        lex.SetEmitSpaces(false);

        return result;

    } else if (function_map_strip_params.find(fn.type) != function_map_strip_params.end()) {
        string result = function_map_strip_params.at(fn.type);

        if (lex.Peek().type == Token::LPARENT) {
            processParentheses(seq, lex); // ignore anything inside ( )
        }

        return result;
    }

    return seq.to_string();
}

string processDate(const StringView seq, Lexer & lex) {
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

    const char * begin = token.data();
    const char * p = begin + token.size() - 1;
    const char * dot = nullptr;
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

string processDateTime(const StringView seq, Lexer & lex) {
    Token data = lex.Consume(Token::STRING);
    if (data.isInvalid()) {
        return seq.to_string();
    } else {
        return string("toDateTime(") + removeMilliseconds(data.literal) + ")";
    }
}

string processEscapeSequencesImpl(const StringView seq, Lexer & lex) {
    string result;

    if (!lex.Match(Token::LCURLY)) {
        return seq.to_string();
    }

    while (true) {
        while (lex.Match(Token::SPACE)) {
        }
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

std::string replaceEscapeSequences(const std::string & query) {
    const char * p = query.c_str();
    const char * end = p + query.size();
    const char * st = p;
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
