#include "hint.h"
#include "text.h"

#include <charconv>

namespace runner {

bool TestHint::expectsError() const {
    return !server_errors.empty() || !client_errors.empty();
}

bool TestHint::matches(int code) const {
    for (const auto c : server_errors) {
        if (c == code)
            return true;
    }
    for (const auto c : client_errors) {
        if (c == code)
            return true;
    }
    return false;
}

std::string TestHint::label() const {
    auto codes = [](const std::vector<int> & list) {
        std::string out;
        for (std::size_t i = 0; i < list.size(); ++i) {
            if (i > 0)
                out += ", ";
            const auto name = errorNameByCode(list[i]);
            out += name ? *name : std::to_string(list[i]);
        }
        return out;
    };
    if (!server_errors.empty() && !client_errors.empty())
        return "error " + codes(server_errors);
    if (!server_errors.empty())
        return "serverError " + codes(server_errors);
    return "clientError " + codes(client_errors);
}

namespace {

    std::vector<std::string> tokenizeHint(std::string_view body) {
        std::vector<std::string> out;
        for (std::size_t i = 0; i < body.size();) {
            const char c = body[i];
            if (isSpace(c)) {
                ++i;
            } else if (c == ',') {
                out.emplace_back(",");
                ++i;
            } else {
                auto j = i;
                while (j < body.size() && !isSpace(body[j]) && body[j] != ',')
                    ++j;
                out.emplace_back(body.substr(i, j - i));
                i = j;
            }
        }
        return out;
    }

    void parseHintBody(TestHint & h, std::string_view body, bool leading) {
        const auto tokens = tokenizeHint(body);
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            const auto & tok = tokens[i];
            if (tok == "echo" || tok == "echoOn") {
                h.echo = true;
            } else if (tok == "echoOff") {
                h.echo = false;
            } else if (tok == "retry") {
                // `retry N until|while serverError X` is used by a few tests that wait for a
                // background process. It is not supported.
                h.parse_error = "the \"retry\" test hint is not supported";
                return;
            } else if (tok == "serverError" || tok == "clientError" || tok == "error") {
                if (leading)
                    continue;
                std::vector<int> codes;
                for (std::size_t k = i + 1; k < tokens.size(); ++k) {
                    const auto & t = tokens[k];
                    if (t == ",")
                        continue;
                    int n = 0;
                    const auto [ptr, ec] = std::from_chars(t.data(), t.data() + t.size(), n);
                    if (ec == std::errc() && ptr == t.data() + t.size()) {
                        codes.push_back(n);
                        continue;
                    }
                    const auto code = errorCodeByName(t);
                    if (!code) {
                        h.parse_error = "unknown error name \"" + t + "\" in test hint";
                        return;
                    }
                    codes.push_back(*code);
                }
                if (tok == "serverError") {
                    h.server_errors = codes;
                } else if (tok == "clientError") {
                    h.client_errors = codes;
                } else {
                    h.server_errors = codes;
                    h.client_errors = codes;
                }
                return;
            }
        }
    }

} // namespace

TestHint parseTestHint(std::string_view full) {
    TestHint h;
    bool leading = true;
    for (std::size_t i = 0; i < full.size();) {
        std::size_t end = 0;
        const char c = full[i];
        if (isSpace(c)) {
            ++i;
            continue;
        }
        if (startsLineComment(full, i)) {
            end = skipLineComment(full, i);
        } else if (full.substr(i).starts_with("/*")) {
            end = skipBlockComment(full, i);
        } else if (c == '\'' || c == '"' || c == '`') {
            leading = false;
            i = skipQuoted(full, i);
            continue;
        } else if (c == '$') {
            leading = false;
            const auto e = heredocEnd(full, i);
            i = (e > 0 ? e : i + 1);
            continue;
        } else {
            leading = false;
            ++i;
            continue;
        }
        const auto comment = full.substr(i, end - i);
        const auto open = comment.find('{');
        if (open != std::string_view::npos) {
            const auto close = comment.find('}', open);
            if (close != std::string_view::npos)
                parseHintBody(h, comment.substr(open + 1, close - open - 1), leading);
        }
        i = end;
    }
    return h;
}

} // namespace runner
