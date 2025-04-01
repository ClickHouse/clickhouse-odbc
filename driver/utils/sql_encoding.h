#pragma once

#include <string>
#include <optional>
#include <type_traits>

// Escapes a SQL literal value for using it in a single-quoted notation in a SQL query.
// Effectively, escapes ' and \ using \.
inline auto escapeForSQL(const std::string & literal) {
    std::string res;
    res.reserve(literal.size() + 10);
    for (auto ch : literal) {
        switch (ch) {
            case '\\':
            case '\'': {
                res += '\\';
                break;
            default:
                break;
            }
        }
        res += ch;
    }
    return res;
}

// toSqlQuery is a family of functions that convert
// different values to strings acceptable
// in SQL statements as values.
inline std::string toSqlQueryValue(const std::string& x)
{
    return "'" + escapeForSQL(x) + "'";
}

template <typename T>
requires std::is_integral_v<T>
inline std::string toSqlQueryValue(T x)
{
    return std::to_string(x);
}

template <typename T>
inline std::string toSqlQueryValue(const std::optional<T>& x)
{
    if (!x.has_value())
        return "NULL";
    return toSqlQueryValue(*x);
}

