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

class ClickHouseException
    : public std::runtime_error
{
public:

    ClickHouseException(std::string message, std::string exception_code_)
        : std::runtime_error(std::move(message))
        , exception_code(std::move(exception_code_)) {}

    const std::string & getExceptionCode() const { return exception_code; }

private:
    std::string exception_code;
};
