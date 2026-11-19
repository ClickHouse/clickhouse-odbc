#include "format.h"
#include "text.h"

#include <unordered_set>

namespace runner {

bool returnsRows(std::string_view sql) {
    // FROM: the FROM-first syntax (FROM t SELECT x) and the pipe syntax (FROM t |> ...).
    // BACKUP and RESTORE return the operation ID and status, KILL returns the killed queries or
    // mutations: tests discard these rows with `FORMAT Null`.
    static const std::unordered_set<std::string> keywords = {
        "SELECT",
        "WITH",
        "SHOW",
        "DESC",
        "DESCRIBE",
        "EXISTS",
        "EXPLAIN",
        "CHECK",
        "FROM",
        "BACKUP",
        "RESTORE",
        "KILL",
    };
    return keywords.contains(leadingKeyword(sql));
}

namespace {

    // Reads the format name after the FORMAT keyword that ends at j.
    bool formatName(std::string_view sql, std::size_t j, std::size_t & end, std::string & name) {
        const auto k = skipWhitespaceAndComments(sql, j);
        if (k == j || k >= sql.size())
            return false;
        if (sql[k] == '`' || sql[k] == '"') {
            // A quoted format name: FORMAT `Null`.
            const auto m = skipQuoted(sql, k);
            end = m;
            return unquoteIdentifier(sql.substr(k, m - k), name);
        }
        auto m = k;
        while (m < sql.size() && isWordByte(sql[m]))
            ++m;
        if (m == k)
            return false;
        end = m;
        name = std::string(sql.substr(k, m - k));
        return true;
    }

    // Tells if the text from i on can follow the FORMAT clause of a query: nothing, a semicolon, or
    // a SETTINGS clause. Otherwise FORMAT is an identifier, as in `SELECT format FROM t`.
    bool endsQuery(std::string_view sql, std::size_t i) {
        const auto k = skipWhitespaceAndComments(sql, i);
        if (k >= sql.size() || sql[k] == ';')
            return true;
        auto m = k;
        while (m < sql.size() && isWordByte(sql[m]))
            ++m;
        return equalsIgnoreCase(sql.substr(k, m - k), "SETTINGS");
    }

    // Finds a top-level `FORMAT <name>` clause. Returns false when there is none.
    bool findFormatClause(std::string_view sql, std::size_t & start, std::size_t & end, std::string & name) {
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
                if (depth == 0 && equalsIgnoreCase(sql.substr(i, j - i), "FORMAT") && (i == 0 || sql[i - 1] != '.')) {
                    // `ORDER BY format DESC` ends the query too: a clause keyword after FORMAT shows an
                    // identifier named `format`.
                    std::size_t m = 0;
                    std::string n;
                    if (formatName(sql, j, m, n) && !isClauseKeyword(n) && endsQuery(sql, m)) {
                        start = i;
                        end = m;
                        name = n;
                        return true;
                    }
                }
                i = j;
            } else {
                ++i;
            }
        }
        return false;
    }

} // namespace

OutputFormat splitOutputFormat(std::string_view sql, std::string & rest) {
    std::size_t start = 0;
    std::size_t end = 0;
    std::string name;
    if (!findFormatClause(sql, start, end, name)) {
        rest = std::string(sql);
        return {};
    }
    OutputFormat f;
    f.name = name;
    f.supported = false;
    // Format names are case-insensitive.
    const auto n = toLower(name);
    if (n == "tabseparated" || n == "tsv") {
        f.supported = true;
    } else if (n == "tabseparatedwithnames" || n == "tsvwithnames") {
        f.supported = f.names = true;
    } else if (n == "tabseparatedwithnamesandtypes" || n == "tsvwithnamesandtypes") {
        f.supported = f.names = f.types = true;
    } else if (n == "tabseparatedraw" || n == "tsvraw" || n == "raw") {
        f.supported = f.raw = true;
    } else if (n == "tabseparatedrawwithnames" || n == "tsvrawwithnames" || n == "rawwithnames") {
        f.supported = f.raw = f.names = true;
    } else if (n == "tabseparatedrawwithnamesandtypes" || n == "tsvrawwithnamesandtypes" || n == "rawwithnamesandtypes") {
        f.supported = f.raw = f.names = f.types = true;
    } else if (n == "null") {
        f.supported = f.discard = true;
    }
    rest = std::string(sql.substr(0, start)) + std::string(sql.substr(end));
    return f;
}

bool isNullFormatResult(std::string_view sqlstate, std::string_view message) {
    // One diagnostic record: "[HY000] Error while processing query <id>: 'Null' format is not supported".
    // The driver names the format as the query wrote it: `FORMAT NULL` gives 'NULL'.
    static constexpr std::string_view suffix = ": 'null' format is not supported";
    return sqlstate == "HY000" && message.find('\n') == std::string_view::npos && message.size() >= suffix.size()
        && equalsIgnoreCase(message.substr(message.size() - suffix.size()), suffix);
}

} // namespace runner
