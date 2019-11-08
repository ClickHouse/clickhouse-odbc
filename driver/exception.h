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
