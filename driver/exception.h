#pragma once

#include "driver/platform/platform.h"

#include <stdexcept>
#include <string>

class SqlException
    : public std::runtime_error
{
public:
    explicit SqlException(const std::string & message_, const std::string & sql_state_ = "HY000", SQLRETURN return_code_ = SQL_ERROR);
    const std::string & getSQLState() const noexcept;
    const SQLRETURN getReturnCode() const noexcept;

private:
    const std::string sql_state;
    const SQLRETURN return_code = SQL_ERROR;
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
