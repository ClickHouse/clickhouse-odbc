#include "driver/exception.h"

SqlException::SqlException(const std::string & message_, const std::string & sql_state_, SQLRETURN return_code_)
    : std::runtime_error(message_)
    , sql_state(sql_state_)
    , return_code(return_code_)
{
}

const std::string & SqlException::getSQLState() const noexcept {
    return sql_state;
}

const SQLRETURN SqlException::getReturnCode() const noexcept {
    return return_code;
}
