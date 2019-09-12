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
    SqlException(const std::string & message_, const std::string & sql_state_ = "HY000");
    const std::string& get_sql_state() const noexcept;

private:
    const std::string sql_state;
};

class DiagnosticHeaderRecord
    : public AttributeContainer
{
public:
    DiagnosticHeaderRecord();
    void reset();
};

class DiagnosticStatusRecord
    : public AttributeContainer
{
public:
    DiagnosticStatusRecord();
    void reset();
};

class DiagnosticsContainer {
public:
    DiagnosticHeaderRecord & get_header();

    void fill(SQLRETURN rc, const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code);
    void fill(const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code);
    void fill(SQLRETURN rc, const std::string& sql_status, const std::string& message);
    void fill(const std::string& sql_status, const std::string& message);

    void reset_header();

    void set_return_code(SQLRETURN rc);
    SQLRETURN get_return_code() const;

    std::size_t get_status_count() const;
    DiagnosticStatusRecord & get_status(std::size_t num);
    void insert_status(DiagnosticStatusRecord && rec);
    void reset_statuses();

private:
    DiagnosticHeaderRecord header;
    std::vector<DiagnosticStatusRecord> statuses;
};
