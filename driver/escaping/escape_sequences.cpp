/*

 https://docs.faircom.com/doc/sqlref/#33384.htm
 https://docs.microsoft.com/ru-ru/sql/odbc/reference/appendixes/time-date-and-interval-functions
 https://my.vertica.com/docs/7.2.x/HTML/index.htm#Authoring/SQLReferenceManual/Functions/Date-Time/TIMESTAMPADD.htm%3FTocPath%3DSQL%2520Reference%2520Manual%7CSQL%2520Functions%7CDate%252FTime%2520Functions%7C_____43

*/
#include "driver/escaping/escape_sequences.h"
#include "driver/escaping/lexer.h"

#include <charconv>
#include <iostream>
#include <map>

using namespace std;

namespace {

const std::map<const std::string, const std::string> fn_convert_map {
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
    { Token::FN_##TOKEN, NAME }

const std::map<const Token::Type, const std::string> function_map {
#include "function_declare.h"
};

#undef DECLARE2

const std::map<const Token::Type, const std::string> function_map_strip_params {
    {Token::FN_CURRENT_TIMESTAMP, "now()"},
};

const std::map<const Token::Type, const std::string> literal_map {
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

const std::map<const Token::Type, const std::string> timeadd_func_map {
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
    } else if (function_map_strip_params.find(token.type) != function_map_strip_params.end()) {
        result += function_map_strip_params.at(token.type);
    } else if ( // any of the remaining recognized FUNCTION( ... ), or any IDENT( ... ), including CAST( ... )
        (token.type == Token::IDENT || function_map.find(token.type) != function_map.end()) &&
        lex.LookAhead(1).type == Token::LPARENT
    ) {
        result += token.literal.to_string();                                            // func name
        lex.Consume();
        result += processParentheses(seq, lex);
    } else if (token.type == Token::NUMBER || token.type == Token::IDENT || token.type == Token::STRING || token.type == Token::PARAM) {
        result += token.literal.to_string();
        lex.Consume();
    } else {
        return "";
    }
    while (lex.Match(Token::SPACE)) {
    }

    return result;
}

string processFunction(const StringView seq, Lexer & lex) {
    const Token fn(lex.Consume());

    if (fn.type == Token::FN_CONVERT) {
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

    } else if (fn.type == Token::FN_BIT_LENGTH) {
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();
        auto expr = processIdentOrFunction(seq, lex);
        if (expr.empty())
            return seq.to_string();
        std::string result = "(length(" + expr +") * 8)";
        if (!lex.Match(Token::RPARENT))
            return seq.to_string();
        return result;
    } else if (fn.type == Token::FN_DIFFERENCE) {
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();
        std::string str1 = processIdentOrFunction(seq, lex);
        if (str1.empty())
            return seq.to_string();
        if (!lex.Match(Token::COMMA))
            return seq.to_string();
        std::string str2 = processIdentOrFunction(seq, lex);
        if (str2.empty())
            return seq.to_string();
        if (!lex.Match(Token::RPARENT))
            return seq.to_string();
        return
            "(equals(substring(soundex(" + str1 + "),1,1), substring(soundex(" + str2 +"),1,1)) +"
            " equals(substring(soundex(" + str1 + "),2,1), substring(soundex(" + str2 +"),2,1)) +"
            " equals(substring(soundex(" + str1 + "),3,1), substring(soundex(" + str2 +"),3,1)) +"
            " equals(substring(soundex(" + str1 + "),4,1), substring(soundex(" + str2 +"),4,1)))";
    } else if (fn.type == Token::FN_INSERT) {
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();
        std::string src = processIdentOrFunction(seq, lex);
        if (src.empty())
            return seq.to_string();
        if (!lex.Match(Token::COMMA))
            return seq.to_string();
        std::string start_str = processIdentOrFunction(seq, lex);
        if (start_str.empty())
            return seq.to_string();
        int64_t start = 0;
        auto start_ec = std::from_chars(start_str.data(), start_str.data() + start_str.size(), start).ec;
        if (start_ec != std::errc{})
            return seq.to_string();
        if (!lex.Match(Token::COMMA))
            return seq.to_string();
        std::string len_str = processIdentOrFunction(seq, lex);
        if (len_str.empty())
            return seq.to_string();
        int64_t len = 0;
        auto len_ec = std::from_chars(len_str.data(), len_str.data() + len_str.size(), len).ec;
        if (len_ec != std::errc{})
            return seq.to_string();
        if (!lex.Match(Token::COMMA))
            return seq.to_string();
        std::string replace = processIdentOrFunction(seq, lex);
        if (replace.empty())
            return seq.to_string();
        if (!lex.Match(Token::RPARENT))
            return seq.to_string();
        if (start <= 0 || len < 0)
            return "cast (NULL, 'Nullable(String)')";
        return "concat( "
            "substringUTF8(" + src + ", 1, " + std::to_string(start - 1) + "), " +
            replace + ", "
            "substringUTF8(" + src + ", " + std::to_string(start + len) + ") )";
    } else if (fn.type == Token::FN_POSITION) {
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();
        std::string needle = processIdentOrFunction(seq, lex);
        if (needle.empty())
            return seq.to_string();
        auto in = lex.Consume();
        if (in.literal != "IN")
            return seq.to_string();
        std::string haystack = processIdentOrFunction(seq, lex);
        if (haystack.empty())
            return seq.to_string();
        if (!lex.Match(Token::RPARENT))
            return seq.to_string();
        return "positionUTF8(" + haystack + ", " + needle + ")";
    } else if (fn.type == Token::FN_TIMESTAMPADD) {
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

        std::string result;
        if (!func.empty()) {
            while (lex.Match(Token::SPACE)) {
            }
            if (!lex.Match(Token::RPARENT)) {
                return seq.to_string();
            }
            result = func + "(" + rdate + ", " + ramount + ")";
        }
        return result;

    } else if (fn.type == Token::FN_LOCATE) {
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
        if (offset.empty()) {
            offset = "1";
        } else {
            lex.Consume();
        }

        // ClickHouse requires `start_pos` to be an unsigned integer,
        // whereas ODBC clients map it as a signed integer. This results in
        // a named parameter such as `{odbc_positional_:Int32}`, which does not
        // match the type required by `locate`. To avoid the illegal type argument
        // error, we cast the offset parameter to UInt64. The `accurateCast` function
        // ensures that the parameter value is never negative.
        std::string result = "locate(" + needle + "," + haystack + ",accurateCast(" + offset + ",'UInt64'))";

        return result;

    } else if (fn.type == Token::FN_LTRIM) {
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();

        auto param = processIdentOrFunction(seq, lex /*, false*/);
        if (param.empty())
            return seq.to_string();
        lex.Consume();
        return "replaceRegexpOne(" + param + ", '^\\\\s+', '')";

    } else if (fn.type == Token::FN_DAYOFWEEK) {
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();

        auto param = processIdentOrFunction(seq, lex /*, false*/);
        if (param.empty())
            return seq.to_string();
        lex.Consume();
        return "if(toDayOfWeek(" + param + ") = 7, 1, toDayOfWeek(" + param + ") + 1)";
/*
    } else if (fn.type == Token::DAYOFYEAR) { // Supported by ClickHouse since 18.13.0
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();

        auto param = processIdentOrFunction(seq, lex);
        if (param.empty())
            return seq.to_string();
        lex.Consume();
        return "( toRelativeDayNum(" + param + ") - toRelativeDayNum(toStartOfYear(" + param + ")) + 1 )";
*/
    } else if (function_map_strip_params.find(fn.type) != function_map_strip_params.end()) {
        string result = function_map_strip_params.at(fn.type);

        if (lex.Peek().type == Token::LPARENT) {
            processParentheses(seq, lex); // ignore anything inside ( )
        }

        return result;
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
            } else if (tok.type == Token::FN_EXTRACT) {
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
