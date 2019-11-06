#pragma once

#include "driver/platform/platform.h"

#include <stdexcept>
#include <string>

class SqlException
    : public std::runtime_error
{
public:
    explicit SqlException(const std::string & message_, const std::string & sql_state_ = "HY000");
    const std::string& getSQLState() const noexcept;

private:
    const std::string sql_state;
};
