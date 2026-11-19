#pragma once

// Lexical helpers for ClickHouse SQL text, as far as `clickhouse-client` needs them to split a
// script into statements and to read test hints. They work on bytes: SQL keywords and the
// characters that matter here are ASCII.

#include <string>
#include <string_view>
#include <vector>

namespace runner {

bool isWordByte(char c);
bool isSpace(char c);

// Each function takes the offset of the opening character and returns the offset after the
// construct, or text.size() when it is not closed.
std::size_t skipQuoted(std::string_view text, std::size_t i);       // '...', "...", `...`
std::size_t skipLineComment(std::string_view text, std::size_t i);  // up to the '\n'
std::size_t skipBlockComment(std::string_view text, std::size_t i); // /* ... */, nested

// Returns the end offset of the `$tag$ ... $tag$` heredoc that starts at i, or 0 when the `$` at
// i does not start one.
std::size_t heredocEnd(std::string_view text, std::size_t i);

bool startsLineComment(std::string_view text, std::size_t i); // --, #!, or "# "

std::size_t skipWhitespace(std::string_view text, std::size_t i);
std::size_t skipWhitespaceAndComments(std::string_view text, std::size_t i);
bool hasSignificantToken(std::string_view text);

// The first word of a statement in upper case, after comments, whitespace and opening
// parentheses.
std::string leadingKeyword(std::string_view sql);

// The words of sql outside quotes, heredocs, comments and parentheses.
std::vector<std::string> topLevelWords(std::string_view sql);

// Tells if the token after head is in an operand position, because head ends with a keyword or an
// operator that needs an operand after it: `SELECT`, `FROM`, `AS`, `,`, `=`. A `format` there is a
// name, not the FORMAT clause: `SELECT format x`, `FROM format f`, `2 * format x`.
bool operandExpected(std::string_view head);

// SQL words that can follow a column or an alias named `format`. No format has such a name.
bool isClauseKeyword(std::string_view word);

bool equalsIgnoreCase(std::string_view a, std::string_view b);
std::string toUpper(std::string_view s);
std::string toLower(std::string_view s);
std::string trimRight(std::string_view s, std::string_view chars = " \t\r\n");
std::string trim(std::string_view s, std::string_view chars = " \t\r\n");

// Removes the quotes of a `...` or "..." identifier, with its escapes. Returns false when s is
// not a quoted identifier.
bool unquoteIdentifier(std::string_view s, std::string & out);

} // namespace runner
