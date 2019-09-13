#include "diagnostics.h"

#include <Poco/Exception.h>

SqlException::SqlException(const std::string & message_, const std::string & sql_state_)
    : std::runtime_error(message_)
    , sql_state(sql_state_)
{
}

const std::string& SqlException::get_sql_state() const noexcept {
    return sql_state;
}

DiagnosticHeaderRecord::DiagnosticHeaderRecord() {
    reset();
}

void DiagnosticHeaderRecord::reset() {
    set_attr(SQL_DIAG_NUMBER, 0);
    set_attr(SQL_DIAG_RETURNCODE, SQL_SUCCESS);
}

DiagnosticStatusRecord::DiagnosticStatusRecord() {
    reset();
}

void DiagnosticStatusRecord::reset() {
}

// WARNING: Do not modify SQL_DIAG_NUMBER field of the header record manually!
DiagnosticHeaderRecord & DiagnosticsContainer::get_header() {
    return header;
}

void DiagnosticsContainer::fill(SQLRETURN rc, const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code) {
    set_return_code(rc);
    fill(sql_status, message, native_error_code);
}

void DiagnosticsContainer::fill(const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code) {
    DiagnosticStatusRecord status;
    status.set_attr(SQL_DIAG_SQLSTATE, sql_status);
    status.set_attr(SQL_DIAG_MESSAGE_TEXT, message);
    status.set_attr(SQL_DIAG_NATIVE, native_error_code);
    insert_status(std::move(status));
}

void DiagnosticsContainer::fill(SQLRETURN rc, const std::string& sql_status, const std::string& message) {
    set_return_code(rc);
    fill(sql_status, message);
}

void DiagnosticsContainer::fill(const std::string& sql_status, const std::string& message) {
    DiagnosticStatusRecord status;
    status.set_attr(SQL_DIAG_SQLSTATE, sql_status);
    status.set_attr(SQL_DIAG_MESSAGE_TEXT, message);
    insert_status(std::move(status));
}

void DiagnosticsContainer::reset_header() {
    header.reset();
    statuses.clear();
}

void DiagnosticsContainer::set_return_code(SQLRETURN rc) {
    header.set_attr(SQL_DIAG_RETURNCODE, rc);
}

SQLRETURN DiagnosticsContainer::get_return_code() const {
    return header.get_attr_as<SQLRETURN>(SQL_DIAG_RETURNCODE);
}

std::size_t DiagnosticsContainer::get_status_count() const {
//  return header.get_attr_as<SQLINTEGER>(SQL_DIAG_NUMBER);
    return statuses.size();
}

DiagnosticStatusRecord & DiagnosticsContainer::get_status(std::size_t num) {
    return statuses.at(num - 1);
}

void DiagnosticsContainer::insert_status(DiagnosticStatusRecord && rec) {
    // TODO: implement proper oredring of status records here.
    statuses.emplace_back(std::move(rec));
    header.set_attr(SQL_DIAG_NUMBER, statuses.size());
}

void DiagnosticsContainer::reset_statuses() {
    statuses.clear();
    header.set_attr(SQL_DIAG_NUMBER, statuses.size());
}
