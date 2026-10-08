#pragma once

#include "driver/platform/platform.h"

#include <stdexcept>
#include <string>

class SqlException
    : public std::runtime_error
{
public:
    SqlException(std::string message_, std::string sql_state_ = "HY000", SQLRETURN return_code_ = SQL_ERROR)
        : std::runtime_error(std::move(message_))
        , sql_state(std::move(sql_state_))
        , return_code(return_code_) {}

    const std::string & getSQLState() const noexcept { return sql_state; };
    SQLRETURN getReturnCode() const noexcept { return return_code; }

private:
    std::string sql_state;
    SQLRETURN return_code;
};

// An error reported by the ClickHouse server, identified by its numeric exception code
// (see `X-ClickHouse-Exception-Code`). Maps the code to an ODBC SQLSTATE.
class ClickHouseException
    : public SqlException
{
public:

    ClickHouseException(std::string message, std::string exception_code_)
        : SqlException(std::move(message), sqlStateForExceptionCode(exception_code_))
        , exception_code(std::move(exception_code_)) {}

    const std::string & getExceptionCode() const { return exception_code; }

private:
    // Codes meaning that the supplied credentials were rejected:
    // 192 UNKNOWN_USER, 193 WRONG_PASSWORD, 194 REQUIRED_PASSWORD, 516 AUTHENTICATION_FAILED, 720 USER_EXPIRED.
    // These are reported as SQLSTATE 28000 "Invalid authorization specification".
    static std::string sqlStateForExceptionCode(const std::string & code) {
        if (code == "192" || code == "193" || code == "194" || code == "516" || code == "720")
            return "28000";
        return "HY000";
    }

    std::string exception_code;
};
