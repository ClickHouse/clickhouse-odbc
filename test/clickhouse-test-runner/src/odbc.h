#pragma once

// A thin layer over the ODBC API: checked calls and the diagnostics of a failed call.

#include <sql.h>
#include <sqlext.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace runner {

// A failed ODBC call, with the diagnostic records of the handle (SQLGetDiagRec).
//
// The driver reports a ClickHouse server error with SQLSTATE HY000 (28000 for an authentication
// error) and the native error 1. The server's error code is only in the message text, which holds
// the server's exception text: `... Code: 60. DB::Exception: ... (UNKNOWN_TABLE)`.
class OdbcError : public std::runtime_error {
public:
    OdbcError(std::string what_, std::string sqlstate_, std::string message_);

    const std::string & sqlState() const {
        return sqlstate;
    }
    const std::string & message() const {
        return diag_message;
    }
    // The ClickHouse error code from `Code: N.` in the message, if any.
    std::optional<int> serverCode() const {
        return server_code;
    }
    // The server's exception text from `Code: N.` on, if any.
    const std::string & serverMessage() const {
        return server_message;
    }

private:
    std::string sqlstate;
    std::string diag_message;
    std::optional<int> server_code;
    std::string server_message;
};

// Reads the diagnostic records of a handle into one message, and the SQLSTATE of the first one.
std::string readDiagnostics(SQLSMALLINT handle_type, SQLHANDLE handle, std::string & sqlstate);

// Throws OdbcError when rc is an error.
void check(SQLRETURN rc, SQLSMALLINT handle_type, SQLHANDLE handle, std::string_view what);

// Finds `Code: N.` in a server error message. Sets text to the message from `Code:` on.
std::optional<int> parseServerCode(std::string_view message, std::string & text);

} // namespace runner
