#include "split.h"
#include "text.h"

namespace runner {

namespace {

    // Tells if a FORMAT that follows head can be the FORMAT clause of an INSERT: head has no
    // top-level SELECT, or a top-level FROM follows its last top-level SELECT. Before that FROM,
    // `format` is in the select list (`SELECT format x FROM s`, `SELECT format DIV 2 FROM s`).
    bool formatAfterSelectFrom(std::string_view head) {
        bool after_select = false;
        bool saw_from = false;
        for (const auto & w : topLevelWords(head)) {
            const auto u = toUpper(w);
            if (u == "SELECT") {
                after_select = true;
                saw_from = false;
            } else if (u == "FROM") {
                saw_from = true;
            }
        }
        return !after_select || saw_from;
    }

    // Tells if the `FORMAT` keyword that ends at format_end starts the inline data of an INSERT
    // statement that begins at start, and sets data_start to the offset after the format name. The
    // `Values` format is excluded: its data ends at `;`, like a query.
    bool insertDataStart(std::string_view text, std::size_t start, std::size_t format_end, std::size_t & data_start) {
        if (leadingKeyword(text.substr(start, format_end - start)) != "INSERT")
            return false;
        auto i = format_end;
        if (i >= text.size() || (text[i] != ' ' && text[i] != '\t' && text[i] != '\n' && text[i] != '\r'))
            return false;
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r'))
            ++i;
        auto j = i;
        while (j < text.size() && isWordByte(text[j]))
            ++j;
        const auto name = text.substr(i, j - i);
        if (j == i || equalsIgnoreCase(name, "Values") || isClauseKeyword(name))
            return false;
        if (!formatAfterSelectFrom(text.substr(start, format_end - 6 - start)))
            return false;
        data_start = j;
        return true;
    }

    // Returns the offset of the `;` that ends the statement at start, or text.size().
    std::size_t scanStatementEnd(std::string_view text, std::size_t start) {
        int parens = 0;
        for (std::size_t i = start; i < text.size();) {
            const char c = text[i];
            if (c == '\'' || c == '"' || c == '`') {
                i = skipQuoted(text, i);
            } else if (c == '-' && text.substr(i).starts_with("--")) {
                i = skipLineComment(text, i);
            } else if (c == '#' && (text.substr(i).starts_with("#!") || text.substr(i).starts_with("# "))) {
                i = skipLineComment(text, i);
            } else if (c == '/' && text.substr(i).starts_with("/*")) {
                i = skipBlockComment(text, i);
            } else if (c == '$') {
                const auto end = heredocEnd(text, i);
                i = (end > 0 ? end : i + 1);
            } else if (c == '(' || c == '[') {
                ++parens;
                ++i;
            } else if (c == ')' || c == ']') {
                --parens;
                ++i;
            } else if (c == ';' && parens <= 0) {
                return i;
            } else if (isWordByte(c)) {
                auto j = i;
                while (j < text.size() && isWordByte(text[j]))
                    ++j;
                std::size_t data_start = 0;
                if (parens <= 0 && equalsIgnoreCase(text.substr(i, j - i), "FORMAT") && insertDataStart(text, start, j, data_start)) {
                    // The inline data ends at an empty line.
                    const auto k = text.find("\n\n", data_start);
                    return k == std::string_view::npos ? text.size() : k;
                }
                i = j;
            } else {
                ++i;
            }
        }
        return text.size();
    }

} // namespace

std::size_t testTagsLength(std::string_view text) {
    std::size_t pos = 0;
    bool first = true;
    while (true) {
        const auto line_start = pos;
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t'))
            ++pos;
        if (!text.substr(pos).starts_with("--"))
            return line_start;
        pos += 2;
        if (first) {
            while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t'))
                ++pos;
            if (!text.substr(pos).starts_with("Tags:"))
                return 0;
            pos += 5;
            first = false;
        }
        const auto eol = text.find_first_of("\r\n", pos);
        if (eol == std::string_view::npos)
            return text.size();
        std::size_t eol_len = 1;
        if (eol + 1 < text.size() && (text[eol + 1] == '\r' || text[eol + 1] == '\n') && text[eol + 1] != text[eol])
            eol_len = 2;
        pos = eol + eol_len;
    }
}

std::vector<Statement> splitStatements(std::string_view text) {
    std::vector<Statement> out;
    auto pos = testTagsLength(text);
    bool echo = false;
    while (true) {
        pos = skipWhitespace(text, pos);
        if (pos >= text.size() || !hasSignificantToken(text.substr(pos)))
            return out;
        const auto begin = pos;
        const auto end = scanStatementEnd(text, pos);
        const auto sql_end = end;
        // Consume the semicolons after the statement.
        pos = end;
        while (pos < text.size() && text[pos] == ';') {
            ++pos;
            const auto next = skipWhitespaceAndComments(text, pos);
            if (next < text.size() && text[next] == ';')
                pos = next;
        }
        // Add the same-line trailing comment when the next statement starts after the end of this
        // line.
        auto newline = text.find('\n', pos);
        if (newline == std::string_view::npos)
            newline = text.size();
        if (newline <= skipWhitespaceAndComments(text, pos))
            pos = newline;

        Statement st;
        // clickhouse-client sends the statement with its semicolon. Tests that read
        // `system.query_log` see it.
        if (sql_end < text.size() && text[sql_end] == ';')
            st.sql = std::string(text.substr(begin, sql_end + 1 - begin));
        else
            st.sql = trimRight(text.substr(begin, sql_end - begin));
        if (sql_end == text.size()) {
            // The last statement has no semicolon: clickhouse-client echoes all the rest of the
            // text, with its trailing newlines.
            st.full = std::string(text.substr(begin));
        } else {
            st.full = trimRight(text.substr(begin, pos - begin));
        }
        st.hint = parseTestHint(st.full);
        if (st.hint.echo)
            echo = *st.hint.echo;
        st.hint.echo_this = echo;
        out.push_back(std::move(st));
    }
}

} // namespace runner
