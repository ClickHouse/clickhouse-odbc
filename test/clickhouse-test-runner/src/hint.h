#pragma once

// Test hints of one statement, as `clickhouse-client` parses them (src/Client/TestHint.cpp):
//
//     SELECT throwIf(1); -- { serverError FUNCTION_THROW_IF_VALUE_IS_NON_ZERO }
//     SELECT 1 / 0;      -- { serverError 153, 6 }
//     -- { echoOn }
//
// An error hint counts only when it is not in a leading comment. An `echo` hint counts in any
// comment.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace runner {

struct TestHint {
    std::vector<int> server_errors;
    std::vector<int> client_errors;
    // Set by `echo`, `echoOn` (true) or `echoOff` (false).
    std::optional<bool> echo;
    // Set when a hint is malformed or names an unknown error.
    std::string parse_error;
    // Tells if this statement is printed before it runs.
    bool echo_this = false;

    bool expectsError() const;
    // Through the driver the server finds every error, also syntax errors that
    // `clickhouse-client` finds itself, so a server error can satisfy a `clientError` hint too.
    bool matches(int code) const;
    std::string label() const;
};

TestHint parseTestHint(std::string_view full);

// The error codes of ClickHouse's src/Common/ErrorCodes.cpp (errorcodes.cpp is generated).
std::optional<int> errorCodeByName(std::string_view name);
std::optional<std::string> errorNameByCode(int code);

} // namespace runner
