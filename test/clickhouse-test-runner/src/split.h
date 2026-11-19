#pragma once

#include "hint.h"

#include <string>
#include <string_view>
#include <vector>

namespace runner {

// One query of a multi-query script, split the way `clickhouse-client` splits it (see
// `ClientBase::executeMultiQuery`).
struct Statement {
    // The text to send to the server: from the first non-whitespace character (leading comments
    // included) to the end of the query, with the semicolon.
    std::string sql;
    // sql plus the semicolon and the same-line trailing comment. The client reads the test hints
    // from this text and prints it for `echo`.
    std::string full;
    TestHint hint;
};

// Splits a script into statements, as `clickhouse-client` does:
//   - The `-- Tags:` header (the first line and the `--` lines after it) is not part of any
//     statement.
//   - A statement ends at a top-level `;`. A comment on the same line after the `;` belongs to
//     that statement; this is where test hints are.
//   - The inline data of `INSERT ... FORMAT <name>` (other than `Values`) ends at an empty line,
//     not at `;`.
//   - Comment-only text after the last statement is not a statement.
std::vector<Statement> splitStatements(std::string_view text);

// The length of the leading `-- Tags: ...` line and the `--` comment lines right after it
// (`getTestTagsLength` in clickhouse-client).
std::size_t testTagsLength(std::string_view text);

} // namespace runner
