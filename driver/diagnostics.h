#pragma once

#include <stdexcept>
#include "log/log.h"
#include "platform.h"

class SqlException : public std::runtime_error {
public:
    SqlException(const std::string & message_, const std::string & state_ = "HY000") : std::runtime_error(message_), state(state_) {}

    std::string sqlState() const {
        return state;
    }

private:
    const std::string state;
};

struct DiagnosticRecord {
    SQLINTEGER native_error_code;
    std::string sql_state;
    std::string message;

    DiagnosticRecord();

    void fromException();
    void reset();
};
