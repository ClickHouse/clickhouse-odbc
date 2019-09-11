#pragma once

#include <stdexcept>
#include "platform.h"

class SqlException : public std::runtime_error {
public:
    SqlException(const std::string & message_, const std::string & state_ = "HY000", RETCODE rc_ = SQL_ERROR)
        : std::runtime_error(message_)
        , state(state_)
        , rc(rc_)
    {}

    std::string sqlState() const {
        return state;
    }

    RETCODE returnCode() const {
        return rc;
    }

private:
    const std::string state;
    RETCODE rc;
};

struct DiagnosticHeaderRecord {
};

struct DiagnosticStatusRecord {
};


struct DiagnosticRecord {










    SQLINTEGER native_error_code;
    std::string sql_state;
    std::string message;

    DiagnosticRecord();

    void fromException();
    void reset();
};
