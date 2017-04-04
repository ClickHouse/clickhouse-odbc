#pragma once

#include "log.h"
#include "platform.h"

struct DiagnosticRecord
{
    SQLINTEGER native_error_code;
    std::string sql_state;
    std::string message;

    DiagnosticRecord();

    void fromException();
    void reset();
};
