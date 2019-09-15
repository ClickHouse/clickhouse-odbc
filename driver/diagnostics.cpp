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

void DiagnosticsContainer::fill_diag(SQLRETURN rc, const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code) {
    set_return_code(rc);
    fill_diag(sql_status, message, native_error_code);
}

void DiagnosticsContainer::fill_diag(const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code) {
    DiagnosticsRecord status;
    status.set_attr(SQL_DIAG_SQLSTATE, sql_status);
    status.set_attr(SQL_DIAG_MESSAGE_TEXT, message);
    status.set_attr(SQL_DIAG_NATIVE, native_error_code);
    insert_diag_status(std::move(status));
}

void DiagnosticsContainer::fill_diag(SQLRETURN rc, const std::string& sql_status, const std::string& message) {
    set_return_code(rc);
    fill_diag(sql_status, message);
}

void DiagnosticsContainer::fill_diag(const std::string& sql_status, const std::string& message) {
    DiagnosticsRecord status;
    status.set_attr(SQL_DIAG_SQLSTATE, sql_status);
    status.set_attr(SQL_DIAG_MESSAGE_TEXT, message);
    insert_diag_status(std::move(status));
}

DiagnosticsRecord & DiagnosticsContainer::get_diag_header() {
    return get_diag_status(0);
}

void DiagnosticsContainer::set_return_code(SQLRETURN rc) {
    get_diag_header().set_attr(SQL_DIAG_RETURNCODE, rc);
}

SQLRETURN DiagnosticsContainer::get_return_code() {
    return get_diag_header().get_attr_as<SQLRETURN>(SQL_DIAG_RETURNCODE, SQL_SUCCESS);
}

std::size_t DiagnosticsContainer::get_diag_status_count() {
    return get_diag_header().get_attr_as<SQLINTEGER>(SQL_DIAG_NUMBER, 0);
}

DiagnosticsRecord & DiagnosticsContainer::get_diag_status(std::size_t num) {
    if (records.empty()) {
        // Initialize at least 0th HEADER record.
        records.reserve(10);
        records.emplace_back();
        records[0].set_attr(SQL_DIAG_NUMBER, 0);
        records[0].set_attr(SQL_DIAG_RETURNCODE, SQL_SUCCESS);
    }

    // Short-circuit on 0th HEADER record.
    if (num == 0) {
        return records[0];
    }

    const auto curr_rec_count = records[0].get_attr_as<SQLINTEGER>(SQL_DIAG_NUMBER, 0) + 1; // +1 for 0th HEADER record

    for (std::size_t i = curr_rec_count; i <= num && i < records.size(); ++i) {
        records[i].reset_attrs();
    }

    while (records.size() < curr_rec_count || records.size() <= num) {
        records.emplace_back();
    }

    if (curr_rec_count <= num) {
        records[0].set_attr(SQL_DESC_COUNT, num);
    }

    return records[num];
}

void DiagnosticsContainer::insert_diag_status(DiagnosticsRecord && rec) {
    // TODO: implement proper ordering of status records here.
    const auto new_rec_count = get_diag_status_count() + 1; // +1 for this new record
    auto & new_rec = get_diag_status(new_rec_count);
    new_rec = std::move(rec);
    get_diag_header().set_attr(SQL_DIAG_NUMBER, new_rec_count);
}

void DiagnosticsContainer::reset_diag() {
    auto & header = get_diag_header();
    header.set_attr(SQL_DIAG_NUMBER, 0);
    header.set_attr(SQL_DIAG_RETURNCODE, SQL_SUCCESS);
}
