/*

 https://docs.faircom.com/doc/sqlref/#33384.htm
 https://docs.microsoft.com/ru-ru/sql/odbc/reference/appendixes/time-date-and-interval-functions
 https://my.vertica.com/docs/7.2.x/HTML/index.htm#Authoring/SQLReferenceManual/Functions/Date-Time/TIMESTAMPADD.htm%3FTocPath%3DSQL%2520Reference%2520Manual%7CSQL%2520Functions%7CDate%252FTime%2520Functions%7C_____43

*/
#include "driver/escaping/escape_sequences.h"
#include "driver/escaping/lexer.h"

#include <charconv>
#include <iostream>
#include <optional>
#include <map>

namespace {

// forward declarations
std::optional<std::string> processFunction(const StringView seq, Lexer & lex);
std::optional<std::string> processEscapeSequencesImpl(const StringView seq, Lexer & lex);

const std::map<const std::string, const std::string> fn_convert_map {
    {"SQL_TINYINT", "toUInt8"},
    {"SQL_SMALLINT", "toUInt16"},
    {"SQL_INTEGER", "toInt32"},
    {"SQL_BIGINT", "toInt64"},
    {"SQL_REAL", "toFloat32"},
    {"SQL_DOUBLE", "toFloat64"},
    {"SQL_CHAR", "toString"},
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

const std::map<const Token::Type, const std::string> literal_map {
    {Token::SQL_TSI_FRAC_SECOND, "'nanosecond'"},
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
    {Token::SQL_TSI_FRAC_SECOND, "addNanoseconds"},
    {Token::SQL_TSI_SECOND, "addSeconds"},
    {Token::SQL_TSI_MINUTE, "addMinutes"},
    {Token::SQL_TSI_HOUR, "addHours"},
    {Token::SQL_TSI_DAY, "addDays"},
    {Token::SQL_TSI_WEEK, "addWeeks"},
    {Token::SQL_TSI_MONTH, "addMonths"},
    {Token::SQL_TSI_QUARTER, "addQuarters"},
    {Token::SQL_TSI_YEAR, "addYears"},
};

std::string convertFunctionByType(const StringView & typeName) {
    const auto type_name_string = typeName.to_string();
    if (fn_convert_map.find(type_name_string) != fn_convert_map.end())
        return fn_convert_map.at(type_name_string);

    return std::string();
}

std::optional<std::string> processParentheses(const StringView seq, Lexer & lex) {
    std::string result;
    lex.SetEmitSpaces(true);
    result += lex.Consume().literal.to_string(); // (

    while (true) {
        const Token token(lex.Peek());
        if (token.type == Token::RPARENT) {
            result += token.literal.to_string();
            lex.Consume();
            break;
        } else if (token.type == Token::LPARENT) {
            auto processed = processParentheses(seq, lex);
            if (!processed) {
                return std::nullopt;
            }
            result += *processed;
        } else if (token.type == Token::LCURLY) {
            lex.SetEmitSpaces(false);
            auto processed = processEscapeSequencesImpl(seq, lex);
            lex.SetEmitSpaces(true);
            if (!processed) {
                return std::nullopt;
            }
            result += *processed;
        } else if (token.type == Token::EOS || token.type == Token::INVALID) {
            break;
        } else {
            result += token.literal.to_string();
            lex.Consume();
        }
    }

    return result;
}

std::optional<std::string> processIdentifierOrFunction(const StringView seq, Lexer & lex) {
    while (lex.Match(Token::SPACE)) {
    }
    const auto token = lex.Peek();
    std::string result;

    if (token.type == Token::LCURLY) {
        lex.SetEmitSpaces(false);
        auto processed = processEscapeSequencesImpl(seq, lex);
        lex.SetEmitSpaces(true);
        if (!processed) {
            return std::nullopt;
        }
        result += *processed;
    } else if (token.type == Token::LPARENT) {
        auto processed = processParentheses(seq, lex);
        if (!processed) {
            return std::nullopt;
        }
        result += *processed;
    } else if ( // any of the remaining recognized FUNCTION( ... ), or any IDENT( ... ), including CAST( ... )
        (token.type == Token::IDENT || function_map.find(token.type) != function_map.end()) &&
        lex.LookAhead(1).type == Token::LPARENT
    ) {
        result += token.literal.to_string();                                            // func name
        lex.Consume();
        auto processed = processParentheses(seq, lex);
        if (!processed) {
            return std::nullopt;
        }
        result += *processed;
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

std::optional<std::string> processFunctionArgument(const StringView seq, Lexer & lex)
{
    std::string result = "";
    lex.SetEmitSpaces(true);
    while (true) {
        const Token tok(lex.Peek());
        switch (tok.type) {
            case (Token::RPARENT):
            case (Token::COMMA):
            case (Token::EOS):
            case (Token::INVALID):
            case (Token::IN_PREP):
                lex.SetEmitSpaces(false);
                return result;
            case (Token::LPARENT): {
                auto processed = processParentheses(seq, lex);
                if (!processed) {
                    return std::nullopt;
                }
                result += *processed;
                break;
            }
            case (Token::LCURLY): {
                lex.SetEmitSpaces(false);
                auto processed = processEscapeSequencesImpl(seq, lex);
                lex.SetEmitSpaces(true);
                if (!processed) {
                    return std::nullopt;
                }
                result += *processed;
                break;
            }
            default:
                result += tok.literal.to_string();
                lex.Consume();
        }
    }
}

std::optional<std::string> processFunction(const StringView seq, Lexer & lex) {
    const Token fn(lex.Consume());

    if (fn.type == Token::FN_CONVERT) {
        std::string result;
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        auto num = processFunctionArgument(seq, lex);
        if (!num)
            return std::nullopt;
        result += *num;
        while (lex.Match(Token::SPACE)) {
        }
        if (!lex.Match(Token::COMMA)) {
            return std::nullopt;
        }
        while (lex.Match(Token::SPACE)) {
        }
        Token type = lex.Consume();
        if (type.type != Token::IDENT) {
            return std::nullopt;
        }
        std::string func = convertFunctionByType(type.literal.to_string());
        if (!func.empty()) {
            while (lex.Match(Token::SPACE)) {
            }
            if (!lex.Match(Token::RPARENT)) {
                return std::nullopt;
            }
            result = func + "(" + result + ")";
        }
        return result;
    } else if (fn.type == Token::FN_BIT_LENGTH) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        auto expr = processFunctionArgument(seq, lex);
        if (!expr)
            return std::nullopt;
        if (!lex.Match(Token::RPARENT))
            return std::nullopt;
        std::string result = "(length(" + *expr +") * 8)";
        return result;
    } else if (fn.type == Token::FN_DIFFERENCE) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        auto str1 = processFunctionArgument(seq, lex);
        if (!str1)
            return std::nullopt;
        if (!lex.Match(Token::COMMA))
            return std::nullopt;
        auto str2 = processFunctionArgument(seq, lex);
        if (!str2)
            return std::nullopt;
        if (!lex.Match(Token::RPARENT))
            return std::nullopt;
        return
            "(equals(substring(soundex(" + *str1 + "),1,1), substring(soundex(" + *str2 +"),1,1)) +"
            " equals(substring(soundex(" + *str1 + "),2,1), substring(soundex(" + *str2 +"),2,1)) +"
            " equals(substring(soundex(" + *str1 + "),3,1), substring(soundex(" + *str2 +"),3,1)) +"
            " equals(substring(soundex(" + *str1 + "),4,1), substring(soundex(" + *str2 +"),4,1)))";
    } else if (fn.type == Token::FN_INSERT) {
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();
        auto src = processFunctionArgument(seq, lex);
        if (!src)
            return std::nullopt;
        if (!lex.Match(Token::COMMA))
            return std::nullopt;
        auto start = processFunctionArgument(seq, lex);
        if (!start)
            return std::nullopt;
        if (!lex.Match(Token::COMMA))
            return std::nullopt;
        auto len = processFunctionArgument(seq, lex);
        if (!len)
            return std::nullopt;
        if (!lex.Match(Token::COMMA))
            return std::nullopt;
        auto replace = processFunctionArgument(seq, lex);
        if (!replace)
            return std::nullopt;
        if (!lex.Match(Token::RPARENT))
            return std::nullopt;
        return "overlayUTF8(" + *src + ", " + *replace + ", " + *start + ", " + *len + ")";
    } else if (fn.type == Token::FN_SPACE) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        auto count = processFunctionArgument(seq, lex);
        if (!count)
            return std::nullopt;
        if (!lex.Match(Token::RPARENT))
            return std::nullopt;
        return "repeat(' ', " + *count + ")";
    } else if (fn.type == Token::FN_POSITION) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        auto needle = processFunctionArgument(seq, lex);
        if (!needle)
            return std::nullopt;
        if (!lex.Match(Token::IN_PREP))
            return std::nullopt;
        auto haystack = processFunctionArgument(seq, lex);
        if (!haystack)
            return std::nullopt;
        if (!lex.Match(Token::RPARENT))
            return std::nullopt;
        return "positionUTF8(" + *haystack + ", " + *needle + ")";
    } else if (fn.type == Token::FN_COT) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        auto expr = processFunctionArgument(seq, lex);
        if (!expr)
            return std::nullopt;
        if (!lex.Match(Token::RPARENT))
            return std::nullopt;
        return "(cos(" + *expr +") / sin (" + *expr + "))";
    } else if (fn.type == Token::FN_CURRENT_TIME || fn.type == Token::FN_CURRENT_TIMESTAMP) {
        if (lex.Match(Token::LPARENT)) {
            auto precision = processFunctionArgument(seq, lex);
            if (!precision)
                return std::nullopt;
            if (!lex.Match(Token::RPARENT))
                return std::nullopt;
            return "now64(" + *precision + ")";
        }
        return "now64()";
    } else if (fn.type == Token::FN_DAYNAME) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        auto date = processFunctionArgument(seq, lex);
        if (!date)
            return std::nullopt;
        if (!lex.Match(Token::RPARENT))
            return std::nullopt;
        return "dateName('weekday', " + *date + ")";
    } else if (fn.type == Token::FN_MONTHNAME) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        auto date = processFunctionArgument(seq, lex);
        if (!date)
            return std::nullopt;
        if (!lex.Match(Token::RPARENT))
            return std::nullopt;
        return "dateName('month', " + *date + ")";
    } else if (fn.type == Token::FN_TIMESTAMPADD) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        Token type = lex.Consume();
        if (timeadd_func_map.find(type.type) == timeadd_func_map.end())
            return std::nullopt;

        auto func_it = timeadd_func_map.find(type.type);
        if (func_it == timeadd_func_map.end())
            return std::nullopt;
        std::string func = func_it->second;
        if (!lex.Match(Token::COMMA))
            return std::nullopt;
        auto ramount = processFunctionArgument(seq, lex);
        if (!ramount)
            return std::nullopt;
        while (lex.Match(Token::SPACE)) {
        }
        if (!lex.Match(Token::COMMA))
            return std::nullopt;
        auto rdate = processFunctionArgument(seq, lex);
        if (!rdate)
            return std::nullopt;

        while (lex.Match(Token::SPACE)) {
        }
        if (!lex.Match(Token::RPARENT)) {
            return std::nullopt;
        }
        return func + "(" + *rdate + ", " + *ramount + ")";

    } else if (fn.type == Token::FN_LOCATE) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        auto needle = processFunctionArgument(seq, lex);
        if (!needle)
            return std::nullopt;
        if (!lex.Match(Token::COMMA))
            return std::nullopt;
        auto haystack = processFunctionArgument(seq, lex);
        if (!haystack)
            return std::nullopt;

        // Last parameter, `offset`, is optional
        std::optional<std::string> offset = "1";
        if (lex.Match(Token::COMMA)) {
            offset = processFunctionArgument(seq, lex);
            if (!offset) {
                return std::nullopt;
            }
        }
        if (!lex.Match(Token::RPARENT)) {
            return std::nullopt;
        }
        return "position(" + *haystack + "," + *needle  + ",accurateCast(" + *offset + ",'UInt64'))";
    } else if (fn.type == Token::FN_LTRIM) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;

        auto param = processFunctionArgument(seq, lex /*, false*/);
        if (!param)
            return std::nullopt;
        lex.Consume();
        return "replaceRegexpOne(" + *param + ", '^\\\\s+', '')";

    } else if (fn.type == Token::FN_DAYOFWEEK) {
        if (!lex.Match(Token::LPARENT))
            return std::nullopt;
        auto param = processFunctionArgument(seq, lex /*, false*/);
        if (!param)
            return std::nullopt;
        lex.Consume();
        return "if(toDayOfWeek(" + *param + ") = 7, 1, toDayOfWeek(" + *param + ") + 1)";
    } else if (function_map.find(fn.type) != function_map.end()) {
        std::string result = function_map.at(fn.type);
        auto func = result;
        lex.SetEmitSpaces(true);
        while (true) {
            const Token tok(lex.Peek());

            if (tok.type == Token::RCURLY) {
                break;
            } else if (tok.type == Token::LCURLY) {
                lex.SetEmitSpaces(false);
                auto processed = processEscapeSequencesImpl(seq, lex);
                lex.SetEmitSpaces(true);
                if (!processed)
                    return std::nullopt;
                result += *processed;
            } else if (tok.type == Token::EOS || tok.type == Token::INVALID) {
                break;
            } else if (tok.type == Token::FN_EXTRACT) {
                auto processed = processFunction(seq, lex);
                if (!processed)
                    return std::nullopt;
                result += *processed;
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

std::optional<std::string> processDate(const StringView seq, Lexer & lex) {
    Token data = lex.Consume(Token::STRING);
    if (data.isInvalid()) {
        return std::nullopt;
    } else {
        return std::string("toDate(") + data.literal.to_string() + ")";
    }
}

std::optional<std::string> processDateTime(const StringView seq, Lexer & lex) {
    Token data = lex.Consume(Token::STRING);
    if (data.isInvalid()) {
        return std::nullopt;
    } else {
        return std::string("toDateTime64(") + data.literal.to_string() + ", 9)";
    }
}

std::optional<std::string> processClickHouseQueryParameter(Token name, const StringView seq, Lexer & lex) {
    bool emit_space = lex.GetEmitSpaces();
    lex.SetEmitSpaces(true);

    std::string result = "{" + name.literal.to_string();
    while (lex.Peek().type != Token::RCURLY) {
        const Token tok = lex.Consume();
        if (tok.type == Token::EOS) {
            return std::nullopt;
        }
        result += tok.literal.to_string();
    }
    result += '}';
    lex.SetEmitSpaces(emit_space);
    return result;
}

std::optional<std::string> processEscapeSequencesImpl(const StringView seq, Lexer & lex) {
    std::string result;

    if (!lex.Match(Token::LCURLY)) {
        return seq.to_string();
    }

    while (true) {
        while (lex.Match(Token::SPACE)) {
        }
        const Token tok(lex.Consume());

        switch (tok.type) {
            case Token::FN: {
                auto processed = processFunction(seq, lex);
                if (!processed)
                    return std::nullopt;
                result += *processed;
                break;
            }

            case Token::D: {
                auto processed = processDate(seq, lex);
                if (!processed)
                    return std::nullopt;
                result += *processed;
                break;
            }

            case Token::TS: {
                auto processed = processDateTime(seq, lex);
                if (!processed)
                    return std::nullopt;
                result += *processed;
                break;
            }

            case Token::RCURLY:
                // End of escape sequence
                return result;

            case Token::T:
                // Unimplemented
                return std::nullopt;

            default:
                // Positional query parameters, just like ODBC escape sequences, also wrapped in
                // curly braces. However they always have a fixed `{name : type}` format, with colon
                // after the parameter name, which does not happen with ODBC escape sequences.
                if (lex.Peek().type == Token::COLON) {
                    auto processed = processClickHouseQueryParameter(tok, seq, lex);
                    if (!processed)
                        return std::nullopt;
                    result += *processed;
                    break;
                }

                return std::nullopt;
        }
    };
}

std::optional<std::string> processEscapeSequences(const StringView seq) {
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
                    // ERROR: unexpected '}' - return the query as it is
                    return query;
                }
                if (--level == 0) {
                    auto processed = processEscapeSequences(StringView(st, p + 1));
                    if (!processed) {
                        // ERROR: could not process the sequence - return the query as it is
                        return query;
                    }
                    ret += *processed;
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
