#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/conversion.h"
#include "driver/utils/type_info.h"
#include "driver/test/common_utils.h"

namespace {

    inline std::string extract_diagnostics(SQLHANDLE handle, SQLSMALLINT type) {
        std::string result;
        SQLSMALLINT i = 0;
        SQLINTEGER native = 0;
        PTChar state[6] = {};     // Exactly 6 char long buffer to store 5 char long SQLSTATE string plus the terminating null.
        PTChar text[10240] = {};  // A reasonably long buffer to store diagnostics messages.
        SQLSMALLINT len = 0;
        SQLRETURN rc = SQL_SUCCESS;

        do {
            rc = SQLGetDiagRec(type, handle, ++i, ptcharCast(state), &native, ptcharCast(text), std::size(text), &len);
            if (SQL_SUCCEEDED(rc)) {
                if (!result.empty())
                    result += '\n';
                result += std::to_string(i) + ":";
                result += "[" + toUTF8(state) + "]";
                result += "[" + std::to_string(native) + "]";
                result += toUTF8(text);
            }
        } while (rc == SQL_SUCCESS);

        if (result.empty() && rc == SQL_INVALID_HANDLE)
            result = "Invalid handle";

        return result;
    }

    inline auto ODBC_CALL_THROW(SQLHANDLE handle, SQLSMALLINT type, const SQLRETURN rc) {
        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error(extract_diagnostics(handle, type));
        return rc;
    }

    inline auto ODBC_CALL_LOG(SQLHANDLE handle, SQLSMALLINT type, const SQLRETURN rc) {
        if (!SQL_SUCCEEDED(rc))
            std::clog << extract_diagnostics(handle, type) << std::endl;
        return rc;
    }

    inline auto ODBC_CALL_ON_ENV_THROW(SQLHENV henv, const SQLRETURN rc) {
        return ODBC_CALL_THROW(henv, SQL_HANDLE_ENV, rc);
    }

    inline auto ODBC_CALL_ON_ENV_LOG(SQLHENV henv, const SQLRETURN rc) {
        return ODBC_CALL_LOG(henv, SQL_HANDLE_ENV, rc);
    }

    inline auto ODBC_CALL_ON_DBC_THROW(SQLHDBC hdbc, const SQLRETURN rc) {
        return ODBC_CALL_THROW(hdbc, SQL_HANDLE_DBC, rc);
    }

    inline auto ODBC_CALL_ON_DBC_LOG(SQLHDBC hdbc, const SQLRETURN rc) {
        return ODBC_CALL_LOG(hdbc, SQL_HANDLE_DBC, rc);
    }

    inline auto ODBC_CALL_ON_STMT_THROW(SQLHSTMT hstmt, const SQLRETURN rc) {
        return ODBC_CALL_THROW(hstmt, SQL_HANDLE_STMT, rc);
    }

    inline auto ODBC_CALL_ON_STMT_LOG(SQLHSTMT hstmt, const SQLRETURN rc) {
        return ODBC_CALL_LOG(hstmt, SQL_HANDLE_STMT, rc);
    }

    inline auto ODBC_CALL_ON_DESC_THROW(SQLHDESC hdesc, const SQLRETURN rc) {
        return ODBC_CALL_THROW(hdesc, SQL_HANDLE_DESC, rc);
    }

    inline auto ODBC_CALL_ON_DESC_LOG(SQLHDESC hdesc, const SQLRETURN rc) {
        return ODBC_CALL_LOG(hdesc, SQL_HANDLE_DESC, rc);
    }

} // namespace
