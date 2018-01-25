/*
 
 https://docs.faircom.com/doc/sqlref/#33384.htm
 https://docs.microsoft.com/ru-ru/sql/odbc/reference/appendixes/time-date-and-interval-functions
 https://my.vertica.com/docs/7.2.x/HTML/index.htm#Authoring/SQLReferenceManual/Functions/Date-Time/TIMESTAMPADD.htm%3FTocPath%3DSQL%2520Reference%2520Manual%7CSQL%2520Functions%7CDate%252FTime%2520Functions%7C_____43

*/
#include "escape_sequences.h"
#include "lexer.h"
#include <map>

#include <iostream>

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
    {Token::SQRT,     "sqrt" },
    {Token::ABS,      "abs" },
    {Token::CONCAT,   "concat" },
    {Token::CURDATE,   "today" },
    {Token::TIMESTAMPDIFF, "dateDiff" },
    //{Token::TIMESTAMPADD, "dateAdd" },
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
        string result;
        if (!lex.Match(Token::LPARENT))
            return seq.to_string();

        const Token num(lex.Peek());

        if (num.type == Token::LCURLY) {
            lex.SetEmitSpaces(false);
            result += processEscapeSequencesImpl(seq, lex);
            lex.SetEmitSpaces(true);
        } else if (num.type != Token::NUMBER && num.type != Token::IDENT) {
            return seq.to_string();
        } else {
            lex.Consume();
            result += num.literal.to_string();
        }

        while (lex.Match(Token::SPACE)) {}

        if (!lex.Match(Token::COMMA)) {
            return seq.to_string();
        }

        while (lex.Match(Token::SPACE)) {}

        Token type = lex.Consume();
        if (type.type != Token::IDENT) {
            return seq.to_string();
        }

        string func = convertFunctionByType(type.literal.to_string());

        if (!func.empty()) {
            while (lex.Match(Token::SPACE)) {}
            if (!lex.Match(Token::RPARENT)) {
                return seq.to_string();
            }
            result = func + "(" + result + ")";
        }

        return result;

    } else if ( fn.type == Token::TIMESTAMPADD ) {
        string result;
        if ( !lex.Match ( Token::LPARENT ) )
            return seq.to_string();

        //const Token num ( lex.Peek() );
        const Token tok(lex.Peek());
        
        Token type = lex.Consume();
/*
        if ( type.type != Token::IDENT ) {
            return seq.to_string();
        }*/
std::cerr << __FUNCTION__ << ":" << __LINE__ << " " << "g0" << "\n";        
        if (timeadd_func_map.find(type.type) == timeadd_func_map.end())
            return seq.to_string(); 
        string func = timeadd_func_map.at(type.type);
        std::cerr << __FUNCTION__ << ":" << __LINE__ << " " << "g1 " << func <<"\n";        
        if ( !lex.Match ( Token::COMMA ) )
            return seq.to_string();
        
        std::cerr << __FUNCTION__ << ":" << __LINE__ << " " << "g2 " << tok.type <<"\n";        
        const auto amount = lex.Consume();
        std::cerr << __FUNCTION__ << ":" << __LINE__ << " " << "g3 " << amount.type <<"\n";        
        string ramount;
        if ( amount.type == Token::LCURLY ) {
            lex.SetEmitSpaces ( false );
            ramount += processEscapeSequencesImpl ( seq, lex );
            lex.SetEmitSpaces ( true );
        } else if ( amount.type != Token::NUMBER && amount.type != Token::IDENT ) {
            return seq.to_string();
        } else {
            ramount += amount.literal.to_string();
            //lex.Consume();
        }
        std::cerr << __FUNCTION__ << ":" << __LINE__ << " " << "g4 t=" << amount.type  << " r=" << result << " c=" << lex.Peek().literal.to_string()<<"\n";        
        
        while ( lex.Match ( Token::SPACE ) ) {}

        if ( !lex.Match ( Token::COMMA ) ) {
            std::cerr << __FUNCTION__ << ":" << __LINE__ << " " << "r5 t=" << lex.Peek().literal.to_string()<<"\n";        
            return seq.to_string();
        }

        while ( lex.Match ( Token::SPACE ) ) {}

        string rdate;
        const auto date = lex.Peek();
        std::cerr << __FUNCTION__ << ":" << __LINE__ << " " << "gg " << date.type << " c=" << lex.Peek().literal.to_string()<<"\n";        
        if ( date.type == Token::LCURLY ) {
            lex.SetEmitSpaces ( false );
            rdate += processEscapeSequencesImpl ( seq, lex );
            lex.SetEmitSpaces ( true );
        } else if ( date.type != Token::NUMBER && date.type != Token::IDENT ) {
            std::cerr << __FUNCTION__ << ":" << __LINE__ << " " << "r6 t=" << lex.Peek().literal.to_string()<<"\n";        
            return seq.to_string();
        } else {
            rdate += date.literal.to_string();
            //lex.Consume();
        }
        std::cerr << __FUNCTION__ << ":" << __LINE__ << " " << "g5 t=" << date.type  << " r=" << result << " c=" << "\n"       ;
        
        if ( !func.empty() ) {
            while ( lex.Match ( Token::SPACE ) ) {}
            if ( !lex.Match ( Token::RPARENT ) ) {
                return seq.to_string();
            }
            result = func + "(" + rdate + ", " + ramount + ")";
        }
        std::cerr << __FUNCTION__ << ":" << __LINE__ << " " << "g7 t="  << " r=" << result << " c=" << "\n"       ;
        
        return result;
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
                if (literal_map.find(tok.type) != literal_map.end()) {
                    result += literal_map.at(tok.type);
                } else
                    result += tok.literal.to_string();
                lex.Consume();
            }
        }
        lex.SetEmitSpaces(false);

        return result;
    } else if (fn.type == Token::TIMESTAMPDIFF) {
        string result = "TSD_tODO";
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
        while (lex.Match(Token::SPACE)) {}
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
