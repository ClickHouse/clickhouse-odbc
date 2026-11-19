#include "odbc.h"
#include "text.h"

#include <charconv>
#include <vector>

namespace runner {

std::optional<int> parseServerCode(std::string_view message, std::string & text) {
    for (std::size_t pos = message.find("Code: "); pos != std::string_view::npos; pos = message.find("Code: ", pos + 1)) {
        const auto begin = pos + 6;
        auto end = begin;
        while (end < message.size() && message[end] >= '0' && message[end] <= '9')
            ++end;
        // `Code: N.` (DB::Exception) or `Code: N, ` (the server's text for other exceptions:
        // `Poco::Exception. Code: 1000, e.code() = ...`, `std::exception. Code: 1001, type: ...`).
        if (end == begin || end >= message.size() || (message[end] != '.' && message.substr(end, 2) != ", "))
            continue;
        int code = 0;
        std::from_chars(message.data() + begin, message.data() + end, code);
        text = trimRight(message.substr(pos));
        return code;
    }
    return std::nullopt;
}

OdbcError::OdbcError(std::string what_, std::string sqlstate_, std::string message_)
    : std::runtime_error(std::move(what_)), sqlstate(std::move(sqlstate_)), diag_message(std::move(message_)) {
    server_code = parseServerCode(diag_message, server_message);
}

std::string readDiagnostics(SQLSMALLINT handle_type, SQLHANDLE handle, std::string & sqlstate) {
    std::string out;
    sqlstate.clear();
    for (SQLSMALLINT i = 1;; ++i) {
        SQLCHAR state[6] = {};
        SQLINTEGER native = 0;
        SQLSMALLINT length = 0;
        std::vector<SQLCHAR> text(1024);
        SQLRETURN rc = SQLGetDiagRec(handle_type, handle, i, state, &native, text.data(), static_cast<SQLSMALLINT>(text.size()), &length);
        if (rc == SQL_SUCCESS_WITH_INFO && length >= static_cast<SQLSMALLINT>(text.size())) {
            text.resize(static_cast<std::size_t>(length) + 1);
            rc = SQLGetDiagRec(handle_type, handle, i, state, &native, text.data(), static_cast<SQLSMALLINT>(text.size()), &length);
        }
        if (!SQL_SUCCEEDED(rc))
            break;
        if (sqlstate.empty())
            sqlstate = reinterpret_cast<const char *>(state);
        if (!out.empty())
            out += '\n';
        out += "[";
        out += reinterpret_cast<const char *>(state);
        out += "] ";
        out += std::string(reinterpret_cast<const char *>(text.data()), std::min<std::size_t>(length, text.size() - 1));
    }
    return out;
}

void check(SQLRETURN rc, SQLSMALLINT handle_type, SQLHANDLE handle, std::string_view what) {
    if (SQL_SUCCEEDED(rc))
        return;
    std::string sqlstate;
    std::string message = readDiagnostics(handle_type, handle, sqlstate);
    if (message.empty())
        message = "no diagnostic record (return code " + std::to_string(rc) + ")";
    throw OdbcError(std::string(what) + ": " + message, sqlstate, message);
}

} // namespace runner
