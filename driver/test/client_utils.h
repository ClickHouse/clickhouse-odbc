#pragma once

#include "driver/platform/platform.h"
#include "driver/type_info.h"

#ifdef NDEBUG
#   define ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(test) test
#else
#   define ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(test) DISABLED_##test
#endif

namespace {

    inline std::string extract_diagnostics(SQLHANDLE handle, SQLSMALLINT type) {
        std::string result;
        SQLSMALLINT i = 0;
        SQLINTEGER native = 0;
        SQLTCHAR state[7] = {};
        SQLTCHAR text[256] = {};
        SQLSMALLINT len = 0;
        SQLRETURN rc = SQL_SUCCESS;

        do {
            rc = SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len );
            if (SQL_SUCCEEDED(rc)) {
                if (!result.empty())
                    result += '\n';
                result += std::to_string(i) + ":";
                result += "[" + std::string((char*)state) + "]";
                result += "[" + std::to_string(native) + "]";
                result += std::string((char*)text);
            }
        } while (rc == SQL_SUCCESS);

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
