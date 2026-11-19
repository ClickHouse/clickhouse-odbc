#include "text.h"

#include <algorithm>
#include <unordered_set>

namespace runner {

bool isWordByte(char c) {
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::size_t skipQuoted(std::string_view text, std::size_t i) {
    const char q = text[i];
    for (std::size_t j = i + 1; j < text.size(); ++j) {
        if (text[j] == '\\') {
            ++j;
        } else if (text[j] == q) {
            // A doubled quote is an escaped quote.
            if (j + 1 < text.size() && text[j + 1] == q) {
                ++j;
                continue;
            }
            return j + 1;
        }
    }
    return text.size();
}

std::size_t skipLineComment(std::string_view text, std::size_t i) {
    const auto nl = text.find('\n', i);
    return nl == std::string_view::npos ? text.size() : nl;
}

std::size_t skipBlockComment(std::string_view text, std::size_t i) {
    // Block comments nest in ClickHouse.
    int depth = 0;
    for (std::size_t j = i; j + 1 < text.size();) {
        if (text[j] == '/' && text[j + 1] == '*') {
            ++depth;
            j += 2;
        } else if (text[j] == '*' && text[j + 1] == '/') {
            --depth;
            j += 2;
            if (depth == 0)
                return j;
        } else {
            ++j;
        }
    }
    return text.size();
}

std::size_t heredocEnd(std::string_view text, std::size_t i) {
    const auto tag_end = text.find('$', i + 1);
    if (tag_end == std::string_view::npos)
        return 0;
    for (std::size_t k = i + 1; k < tag_end; ++k) {
        if (!isWordByte(text[k]))
            return 0;
    }
    const auto delim = text.substr(i, tag_end + 1 - i);
    const auto end = text.find(delim, tag_end + 1);
    if (end == std::string_view::npos)
        return 0;
    return end + delim.size();
}

bool startsLineComment(std::string_view text, std::size_t i) {
    const auto rest = text.substr(i);
    return rest.starts_with("--") || rest.starts_with("#!") || rest.starts_with("# ");
}

std::size_t skipWhitespace(std::string_view text, std::size_t i) {
    while (i < text.size() && isSpace(text[i]))
        ++i;
    return i;
}

std::size_t skipWhitespaceAndComments(std::string_view text, std::size_t i) {
    while (i < text.size()) {
        if (isSpace(text[i]))
            ++i;
        else if (startsLineComment(text, i))
            i = skipLineComment(text, i);
        else if (text.substr(i).starts_with("/*"))
            i = skipBlockComment(text, i);
        else
            return i;
    }
    return i;
}

bool hasSignificantToken(std::string_view text) {
    return skipWhitespaceAndComments(text, 0) < text.size();
}

std::string leadingKeyword(std::string_view sql) {
    auto i = skipWhitespaceAndComments(sql, 0);
    while (i < sql.size() && (sql[i] == '(' || isSpace(sql[i])))
        i = skipWhitespaceAndComments(sql, i + 1);
    auto j = i;
    while (j < sql.size() && isWordByte(sql[j]))
        ++j;
    return toUpper(sql.substr(i, j - i));
}

std::vector<std::string> topLevelWords(std::string_view sql) {
    std::vector<std::string> words;
    int depth = 0;
    for (std::size_t i = 0; i < sql.size();) {
        const char c = sql[i];
        if (c == '\'' || c == '"' || c == '`') {
            i = skipQuoted(sql, i);
        } else if (startsLineComment(sql, i)) {
            i = skipLineComment(sql, i);
        } else if (sql.substr(i).starts_with("/*")) {
            i = skipBlockComment(sql, i);
        } else if (c == '$') {
            const auto e = heredocEnd(sql, i);
            i = (e > 0 ? e : i + 1);
        } else if (c == '(' || c == '[') {
            ++depth;
            ++i;
        } else if (c == ')' || c == ']') {
            --depth;
            ++i;
        } else if (isWordByte(c)) {
            auto j = i;
            while (j < sql.size() && isWordByte(sql[j]))
                ++j;
            if (depth == 0)
                words.emplace_back(sql.substr(i, j - i));
            i = j;
        } else {
            ++i;
        }
    }
    return words;
}

namespace {

    // Keywords that need an operand after them: an expression, a table or an alias.
    bool needsOperand(std::string_view word) {
        static const std::unordered_set<std::string> keywords = {
            "SELECT",
            "DISTINCT",
            "FROM",
            "JOIN",
            "WHERE",
            "PREWHERE",
            "HAVING",
            "QUALIFY",
            "BY",
            "ON",
            "USING",
            "AS",
            "AND",
            "OR",
            "NOT",
            "IN",
            "LIKE",
            "ILIKE",
            "BETWEEN",
            "CASE",
            "WHEN",
            "THEN",
            "ELSE",
            "DIV",
            "MOD",
            "REGEXP",
            "LIMIT",
            "OFFSET",
        };
        return keywords.contains(toUpper(word));
    }

    // Keywords of needsOperand that can also start an operand: `SELECT DISTINCT x`, `AND NOT x`,
    // `SELECT CASE WHEN x`. Any other word in an operand position is a name (`AS mod`, `BY offset`).
    bool startsOperand(std::string_view word) {
        static const std::unordered_set<std::string> keywords = {"SELECT", "DISTINCT", "NOT", "CASE", "WHEN"};
        return keywords.contains(toUpper(word));
    }

    // A comma, or a character of an operator. `*` is handled apart: it can be the asterisk.
    bool needsOperandAfter(char c) {
        return std::string_view(",=<>!+-/%|&?:^").find(c) != std::string_view::npos;
    }

} // namespace

bool operandExpected(std::string_view head) {
    bool expected = false;
    for (std::size_t i = 0; i < head.size();) {
        const char c = head[i];
        if (c == '\'' || c == '"' || c == '`') {
            i = skipQuoted(head, i);
            expected = false;
        } else if (startsLineComment(head, i)) {
            i = skipLineComment(head, i);
        } else if (head.substr(i).starts_with("/*")) {
            i = skipBlockComment(head, i);
        } else if (c == '$') {
            const auto e = heredocEnd(head, i);
            i = (e > 0 ? e : i + 1);
            expected = false;
        } else if (isWordByte(c)) {
            auto j = i;
            while (j < head.size() && isWordByte(head[j]))
                ++j;
            // A word after a dot is a part of a qualified name (`db.table`), not a keyword.
            const auto w = head.substr(i, j - i);
            expected = (i == 0 || head[i - 1] != '.') && needsOperand(w) && (!expected || startsOperand(w));
            i = j;
        } else {
            if (c == '*') {
                // After an operand, `*` is the multiplication. Otherwise it is the asterisk:
                // `SELECT *`, `t.*`.
                expected = !expected && !(i > 0 && head[i - 1] == '.');
            } else if (c == '(' || c == '[') {
                expected = true;
            } else if (c == ')' || c == ']') {
                expected = false;
            } else if (!isSpace(c)) {
                expected = needsOperandAfter(c);
            }
            ++i;
        }
    }
    return expected;
}

bool isClauseKeyword(std::string_view word) {
    static const std::unordered_set<std::string> keywords = {
        "FROM",
        "WHERE",
        "PREWHERE",
        "AS",
        "AND",
        "OR",
        "NOT",
        "IN",
        "IS",
        "LIKE",
        "ILIKE",
        "BETWEEN",
        "GROUP",
        "ORDER",
        "LIMIT",
        "OFFSET",
        "HAVING",
        "QUALIFY",
        "WINDOW",
        "UNION",
        "EXCEPT",
        "INTERSECT",
        "JOIN",
        "INNER",
        "LEFT",
        "RIGHT",
        "FULL",
        "CROSS",
        "ARRAY",
        "ON",
        "USING",
        "SETTINGS",
        "INTO",
        "ASC",
        "DESC",
        "NULLS",
        "COLLATE",
        "WITH",
        "THEN",
        "ELSE",
        "END",
        "WHEN",
        "OVER",
        "GLOBAL",
        "DIV",
        "MOD",
        "REGEXP",
        "FINAL",
        "SAMPLE",
        "FILL",
        "TIES",
        "BY",
    };
    return keywords.contains(toUpper(word));
}

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
        return (x >= 'a' && x <= 'z' ? x - 32 : x) == (y >= 'a' && y <= 'z' ? y - 32 : y);
    });
}

std::string toUpper(std::string_view s) {
    std::string out(s);
    for (auto & c : out) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    }
    return out;
}

std::string toLower(std::string_view s) {
    std::string out(s);
    for (auto & c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string trimRight(std::string_view s, std::string_view chars) {
    const auto end = s.find_last_not_of(chars);
    return end == std::string_view::npos ? std::string() : std::string(s.substr(0, end + 1));
}

std::string trim(std::string_view s, std::string_view chars) {
    const auto begin = s.find_first_not_of(chars);
    if (begin == std::string_view::npos)
        return {};
    return trimRight(s.substr(begin), chars);
}

bool unquoteIdentifier(std::string_view s, std::string & out) {
    if (s.size() < 2 || (s.front() != '`' && s.front() != '"') || s.back() != s.front())
        return false;
    const char q = s.front();
    out.clear();
    for (std::size_t i = 1; i + 1 < s.size(); ++i) {
        const char c = s[i];
        if (c == '\\' && i + 2 < s.size()) {
            ++i;
            out += s[i];
        } else if (c == q && i + 2 < s.size() && s[i + 1] == q) {
            ++i;
            out += q;
        } else {
            out += c;
        }
    }
    return true;
}

} // namespace runner
