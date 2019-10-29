#include "driver/diagnostics.h"

#include <Poco/Exception.h>

#include <algorithm>

SqlException::SqlException(const std::string & message_, const std::string & sql_state_)
    : std::runtime_error(message_)
    , sql_state(sql_state_)
{
}

const std::string& SqlException::getSQLState() const noexcept {
    return sql_state;
}

void DiagnosticsContainer::fillDiag(SQLRETURN rc, const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code) {
    setReturnCode(rc);
    fillDiag(sql_status, message, native_error_code);
}

void DiagnosticsContainer::fillDiag(const std::string& sql_status, const std::string& message, SQLINTEGER native_error_code) {
    DiagnosticsRecord status;
    status.setAttr(SQL_DIAG_SQLSTATE, sql_status);
    status.setAttr(SQL_DIAG_MESSAGE_TEXT, message);
    status.setAttr(SQL_DIAG_NATIVE, native_error_code);
    insertDiagStatus(std::move(status));
}

void DiagnosticsContainer::fillDiag(SQLRETURN rc, const std::string& sql_status, const std::string& message) {
    setReturnCode(rc);
    fillDiag(sql_status, message);
}

void DiagnosticsContainer::fillDiag(const std::string& sql_status, const std::string& message) {
    DiagnosticsRecord status;
    status.setAttr(SQL_DIAG_SQLSTATE, sql_status);
    status.setAttr(SQL_DIAG_MESSAGE_TEXT, message);
    insertDiagStatus(std::move(status));
}

DiagnosticsRecord & DiagnosticsContainer::getDiagHeader() {
    return getDiagStatus(0);
}

void DiagnosticsContainer::setReturnCode(SQLRETURN rc) {
    getDiagHeader().setAttr(SQL_DIAG_RETURNCODE, rc);
}

SQLRETURN DiagnosticsContainer::getReturnCode() {
    return getDiagHeader().getAttrAs<SQLRETURN>(SQL_DIAG_RETURNCODE, SQL_SUCCESS);
}

std::size_t DiagnosticsContainer::getDiagStatusCount() {
    return getDiagHeader().getAttrAs<SQLINTEGER>(SQL_DIAG_NUMBER, 0);
}

DiagnosticsRecord & DiagnosticsContainer::getDiagStatus(std::size_t num) {
    if (records.empty()) {
        // Initialize at least 0th HEADER record.
        records.reserve(10);
        records.emplace_back();
        records[0].setAttr(SQL_DIAG_NUMBER, 0);
        records[0].setAttr(SQL_DIAG_RETURNCODE, SQL_SUCCESS);
    }

    // Short-circuit on 0th HEADER record.
    if (num == 0) {
        return records[0];
    }

    const std::size_t curr_rec_count = records[0].getAttrAs<SQLINTEGER>(SQL_DIAG_NUMBER, 0);

    for (std::size_t i = curr_rec_count + 1; i <= num && i < records.size(); ++i) {
        records[i].resetAttrs();
    }

    while (records.size() <= std::max(curr_rec_count, num)) {
        records.emplace_back();
    }

    if (curr_rec_count < num) {
        records[0].setAttr(SQL_DIAG_NUMBER, num);
    }

    return records[num];
}

void DiagnosticsContainer::insertDiagStatus(DiagnosticsRecord && rec) {
    // TODO: implement proper ordering of status records here.
    const auto new_rec_count = getDiagStatusCount() + 1; // +1 for this new record
    auto & new_rec = getDiagStatus(new_rec_count);
    new_rec = std::move(rec);
}

void DiagnosticsContainer::resetDiag() {
    auto & header = getDiagHeader();
    header.setAttr(SQL_DIAG_NUMBER, 0);
    header.setAttr(SQL_DIAG_RETURNCODE, SQL_SUCCESS);
}
