#pragma once

#include "platform.h"
#include "utils.h"
#include "attributes.h"

#include <stdexcept>
#include <string>
#include <vector>

class SqlException
    : public std::runtime_error
{
public:
    explicit SqlException(const std::string & message_, const std::string & sql_state_ = "HY000");
    const std::string& getSQLState() const noexcept;

private:
    const std::string sql_state;
};

class DiagnosticsRecord
    : public AttributeContainer
{
public:
};

class DiagnosticsContainer {
public:
    void fillDiag(SQLRETURN rc, const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code);
    void fillDiag(const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code);
    void fillDiag(SQLRETURN rc, const std::string& sql_status, const std::string& message);
    void fillDiag(const std::string& sql_status, const std::string& message);

    DiagnosticsRecord & getDiagHeader();

    void setReturnCode(SQLRETURN rc);
    SQLRETURN getReturnCode();

    std::size_t getDiagStatusCount();
    DiagnosticsRecord & getDiagStatus(std::size_t num);
    void insertDiagStatus(DiagnosticsRecord && rec);

    void resetDiag();

private:
    std::vector<DiagnosticsRecord> records;
};
