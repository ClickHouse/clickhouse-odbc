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
    const std::string& get_sql_state() const noexcept;

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
    void fill_diag(SQLRETURN rc, const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code);
    void fill_diag(const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code);
    void fill_diag(SQLRETURN rc, const std::string& sql_status, const std::string& message);
    void fill_diag(const std::string& sql_status, const std::string& message);

    DiagnosticsRecord & get_diag_header();

    void set_return_code(SQLRETURN rc);
    SQLRETURN get_return_code();

    std::size_t get_diag_status_count();
    DiagnosticsRecord & get_diag_status(std::size_t num);
    void insert_diag_status(DiagnosticsRecord && rec);

    void reset_diag();

private:
    std::vector<DiagnosticsRecord> records;
};
