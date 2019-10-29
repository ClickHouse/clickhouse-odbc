#include "driver/utils/utils.h"
#include "driver/utils/scope_guard.h"
#include "driver/environment.h"
#include "driver/connection.h"
#include "driver/statement.h"

#include <Poco/Net/HTTPClientSession.h>

namespace { namespace impl {

SQLRETURN SetEnvAttr(
    SQLHENV environment_handle,
    SQLINTEGER attribute,
    SQLPOINTER value,
    SQLINTEGER value_length
) noexcept {
    auto func = [&](Environment & environment) {
        LOG("SetEnvAttr: " << attribute);

        switch (attribute) {
            case SQL_ATTR_CONNECTION_POOLING:
            case SQL_ATTR_CP_MATCH:
            case SQL_ATTR_OUTPUT_NTS:
                return SQL_SUCCESS;

            case SQL_ATTR_ODBC_VERSION: {
                intptr_t int_value = reinterpret_cast<intptr_t>(value);
                if (int_value != SQL_OV_ODBC2 && int_value != SQL_OV_ODBC3
#if defined(SQL_OV_ODBC3_80)
                    && int_value != SQL_OV_ODBC3_80
#endif
                )
                    throw std::runtime_error("Unsupported ODBC version." + std::to_string(int_value));

                environment.odbc_version = int_value;
                LOG("Set ODBC version to " << int_value);

                return SQL_SUCCESS;
            }

            default:
                LOG("SetEnvAttr: Unsupported attribute " << attribute);
                //throw std::runtime_error("Unsupported environment attribute.");
                return SQL_ERROR;
        }
    };

    return CALL_WITH_HANDLE(environment_handle, func);
}

SQLRETURN GetEnvAttr(
    SQLHENV environment_handle,
    SQLINTEGER attribute,
    SQLPOINTER out_value,
    SQLINTEGER out_value_max_length,
    SQLINTEGER * out_value_length
) noexcept {
    auto func = [&](Environment & environment) -> SQLRETURN {
        LOG("GetEnvAttr: " << attribute);

        switch (attribute) {
            case SQL_ATTR_ODBC_VERSION:
                fillOutputNumber<SQLUINTEGER>(environment.odbc_version, out_value, SQLINTEGER{0}/* out_value_max_length */, out_value_length);
                return SQL_SUCCESS;

            case SQL_ATTR_CONNECTION_POOLING:
            case SQL_ATTR_CP_MATCH:
            case SQL_ATTR_OUTPUT_NTS:
            default:
                LOG("GetEnvAttr: Unsupported attribute " << attribute);
                //throw std::runtime_error("Unsupported environment attribute.");
                return SQL_ERROR;
        }
    };

    return CALL_WITH_HANDLE(environment_handle, func);
}

SQLRETURN SetConnectAttr(
    SQLHDBC connection_handle,
    SQLINTEGER attribute,
    SQLPOINTER value,
    SQLINTEGER value_length
) noexcept {
    auto func = [&](Connection & connection) {
        LOG("SetConnectAttr: " << attribute << " = " << value << " (" << value_length << ")");

        switch (attribute) {
            case SQL_ATTR_CONNECTION_TIMEOUT: {
                auto connection_timeout = static_cast<SQLUSMALLINT>(reinterpret_cast<intptr_t>(value));
                LOG("Set connection timeout: " << connection_timeout);
                connection.connection_timeout = connection.timeout;
                if (connection.session)
                    connection.session->setTimeout(Poco::Timespan(connection.connection_timeout, 0),
                        Poco::Timespan(connection.timeout, 0),
                        Poco::Timespan(connection.timeout, 0));
                return SQL_SUCCESS;
            }

            case SQL_ATTR_CURRENT_CATALOG:
                connection.setDatabase(stringFromSQLBytes((SQLTCHAR *)value, value_length));
                return SQL_SUCCESS;

            case SQL_ATTR_ANSI_APP:
                return SQL_ERROR;

            case SQL_ATTR_TRACE: {
                if (value == reinterpret_cast<SQLPOINTER>(SQL_OPT_TRACE_ON)) {
                    connection.getDriver().setAttr(SQL_ATTR_TRACE, SQL_OPT_TRACE_ON);
                }
                else if (value == reinterpret_cast<SQLPOINTER>(SQL_OPT_TRACE_OFF)) {
                    connection.getDriver().setAttr(SQL_ATTR_TRACE, SQL_OPT_TRACE_OFF);
                }
                else {
                    LOG("SetConnectAttr: SQL_ATTR_TRACE: Unknown value " << value);
                    return SQL_ERROR;
                }
                return SQL_SUCCESS;
            }

            case SQL_ATTR_TRACEFILE: {
                const std::string tracefile = stringFromSQLBytes((SQLTCHAR *)value, value_length);
                connection.getDriver().setAttr(SQL_ATTR_TRACEFILE, tracefile);
                return SQL_SUCCESS;
            }

#if defined(SQL_APPLICATION_NAME)
            case SQL_APPLICATION_NAME: {
                auto string = stringFromSQLBytes((SQLTCHAR *)value, value_length);
                LOG("SetConnectAttr: SQL_APPLICATION_NAME: " << string);
                connection.useragent = string;
                return SQL_SUCCESS;
            }
#endif

            case SQL_ATTR_METADATA_ID:
                connection.setAttr(SQL_ATTR_METADATA_ID, value);
                return SQL_SUCCESS;

            case SQL_ATTR_ACCESS_MODE:
            case SQL_ATTR_ASYNC_ENABLE:
            case SQL_ATTR_AUTO_IPD:
            case SQL_ATTR_AUTOCOMMIT:
            case SQL_ATTR_CONNECTION_DEAD:
            case SQL_ATTR_LOGIN_TIMEOUT: // We have no special login procedure - cant set login timeout separately
            case SQL_ATTR_ODBC_CURSORS:
            case SQL_ATTR_PACKET_SIZE:
            case SQL_ATTR_QUIET_MODE:
            case SQL_ATTR_TRANSLATE_LIB:
            case SQL_ATTR_TRANSLATE_OPTION:
            case SQL_ATTR_TXN_ISOLATION:
                return SQL_SUCCESS;

            default:
                LOG("SetConnectAttr: Unsupported attribute " << attribute);
                //throw SqlException("Unsupported connection attribute.", "HY092");
                return SQL_ERROR;
        }
    };

    return CALL_WITH_HANDLE(connection_handle, func);
}

SQLRETURN GetConnectAttr(
    SQLHDBC connection_handle,
    SQLINTEGER attribute,
    SQLPOINTER out_value,
    SQLINTEGER out_value_max_length,
    SQLINTEGER * out_value_length
) noexcept {
    auto func = [&](Connection & connection) -> SQLRETURN {
        LOG("GetConnectAttr: " << attribute);

        const char * name = nullptr;

        switch (attribute) {
            CASE_NUM(SQL_ATTR_CONNECTION_DEAD, SQLUINTEGER, SQL_CD_FALSE);
            CASE_FALLTHROUGH(SQL_ATTR_CONNECTION_TIMEOUT);
            CASE_NUM(
                SQL_ATTR_LOGIN_TIMEOUT, SQLUSMALLINT, connection.session ? connection.session->getTimeout().seconds() : connection.timeout);
            CASE_NUM(SQL_ATTR_TXN_ISOLATION, SQLINTEGER, SQL_TXN_SERIALIZABLE); // mssql linked server
            CASE_NUM(SQL_ATTR_AUTOCOMMIT, SQLINTEGER, SQL_AUTOCOMMIT_ON);
            CASE_NUM(SQL_ATTR_TRACE, SQLINTEGER, (connection.getDriver().isLoggingEnabled() ? SQL_OPT_TRACE_ON : SQL_OPT_TRACE_OFF));

            case SQL_ATTR_CURRENT_CATALOG:
                fillOutputPlatformString(connection.getDatabase(), out_value, out_value_max_length, out_value_length);
                return SQL_SUCCESS;

            case SQL_ATTR_ANSI_APP:
                return SQL_ERROR;

            case SQL_ATTR_TRACEFILE: {
                const auto tracefile = connection.getDriver().getAttrAs<std::string>(SQL_ATTR_TRACEFILE);
                fillOutputPlatformString(tracefile, out_value, out_value_max_length, out_value_length);
                return SQL_SUCCESS;
            }

            case SQL_ATTR_METADATA_ID:
                return fillOutputNumber<SQLUINTEGER>(
                    connection.getAttrAs<SQLUINTEGER>(SQL_ATTR_METADATA_ID, SQL_FALSE),
                    out_value, SQLINTEGER{0}/* out_value_max_length */, out_value_length
                );

            case SQL_ATTR_ACCESS_MODE:
            case SQL_ATTR_ASYNC_ENABLE:
            case SQL_ATTR_AUTO_IPD:
            case SQL_ATTR_ODBC_CURSORS:
            case SQL_ATTR_PACKET_SIZE:
            case SQL_ATTR_QUIET_MODE:
            case SQL_ATTR_TRANSLATE_LIB:
            case SQL_ATTR_TRANSLATE_OPTION:
            default:
                LOG("GetConnectAttr: Unsupported attribute " << attribute);
                //throw std::runtime_error("Unsupported connection attribute.");
                return SQL_ERROR;
        }

        return SQL_SUCCESS;
    };

    return CALL_WITH_HANDLE(connection_handle, func);
}

SQLRETURN setDescriptorHandle(Statement & statement, SQLINTEGER descriptor_type, SQLHANDLE descriptor_handle) {
    switch (descriptor_type) {
        case SQL_ATTR_APP_ROW_DESC:
        case SQL_ATTR_APP_PARAM_DESC:
            if (descriptor_handle == 0) {
                statement.setImplicitDescriptor(descriptor_type);
                return SQL_SUCCESS;
            }
            break;
        case SQL_ATTR_IMP_ROW_DESC:   /* 10012 (read-only) */
        case SQL_ATTR_IMP_PARAM_DESC: /* 10013 (read-only) */
            return SQL_ERROR;
    }

    // We don't want to modify the diagnostics info of the descriptor instance itself,
    // but we want to forward it to the current statement context unchanged, so we are going to
    // intercept exceptions and process them outside the descriptor dispatch closure.
    std::exception_ptr ex;

    auto func = [&] (Descriptor & descriptor) {
        try {
            if (descriptor.getParent().getHandle() != statement.getParent().getHandle())
                throw SqlException("Invalid attribute value", "HY024");

            if (descriptor.getAttrAs<SQLSMALLINT>(SQL_DESC_ALLOC_TYPE) == SQL_DESC_ALLOC_AUTO)
                throw SqlException("Invalid use of an automatically allocated descriptor handle", "HY017");

            switch (descriptor_type) {
                case SQL_ATTR_APP_ROW_DESC:
                case SQL_ATTR_APP_PARAM_DESC:
                    statement.setExplicitDescriptor(descriptor_type, descriptor.shared_from_this());
                    return SQL_SUCCESS;
            }
        }
        catch (...) {
            ex = std::current_exception();
        }

        return SQL_ERROR;
    };

    auto rc = CALL_WITH_HANDLE_SKIP_DIAG(descriptor_handle, func);

    if (ex)
        std::rethrow_exception(ex);

    if (rc == SQL_INVALID_HANDLE)
        throw SqlException("Invalid attribute value", "HY024");

    return rc;
}

SQLRETURN SetStmtAttr(
    SQLHSTMT statement_handle,
    SQLINTEGER attribute,
    SQLPOINTER value,
    SQLINTEGER value_length
) noexcept {
    auto func = [&](Statement & statement) -> SQLRETURN {
        LOG("SetStmtAttr: " << attribute << " value=" << value << " value_length=" << value_length);

        switch (attribute) {

#define CASE_SET_IN_DESC(STMT_ATTR, DESC_TYPE, DESC_ATTR, VALUE_TYPE) \
            case STMT_ATTR: \
                statement.getEffectiveDescriptor(DESC_TYPE).setAttr(DESC_ATTR, reinterpret_cast<VALUE_TYPE>(value)); \
                return SQL_SUCCESS;

            CASE_SET_IN_DESC(SQL_ATTR_PARAM_BIND_OFFSET_PTR, SQL_ATTR_APP_PARAM_DESC, SQL_DESC_BIND_OFFSET_PTR, SQLULEN *);
            CASE_SET_IN_DESC(SQL_ATTR_PARAM_BIND_TYPE, SQL_ATTR_APP_PARAM_DESC, SQL_DESC_BIND_TYPE, SQLULEN);
            CASE_SET_IN_DESC(SQL_ATTR_PARAM_OPERATION_PTR, SQL_ATTR_APP_PARAM_DESC, SQL_DESC_ARRAY_STATUS_PTR, SQLUSMALLINT *);
            CASE_SET_IN_DESC(SQL_ATTR_PARAM_STATUS_PTR, SQL_ATTR_IMP_PARAM_DESC, SQL_DESC_ARRAY_STATUS_PTR, SQLUSMALLINT *);
            CASE_SET_IN_DESC(SQL_ATTR_PARAMS_PROCESSED_PTR, SQL_ATTR_IMP_PARAM_DESC, SQL_DESC_ROWS_PROCESSED_PTR, SQLULEN *);
            CASE_SET_IN_DESC(SQL_ATTR_PARAMSET_SIZE, SQL_ATTR_APP_PARAM_DESC, SQL_DESC_ARRAY_SIZE, SQLULEN);
            CASE_SET_IN_DESC(SQL_ATTR_ROW_ARRAY_SIZE, SQL_ATTR_APP_ROW_DESC, SQL_DESC_ARRAY_SIZE, SQLULEN);
            CASE_SET_IN_DESC(SQL_ATTR_ROW_BIND_OFFSET_PTR, SQL_ATTR_APP_ROW_DESC, SQL_DESC_BIND_OFFSET_PTR, SQLULEN *);
            CASE_SET_IN_DESC(SQL_ATTR_ROW_BIND_TYPE, SQL_ATTR_APP_ROW_DESC, SQL_DESC_BIND_TYPE, SQLULEN);
            CASE_SET_IN_DESC(SQL_ATTR_ROW_OPERATION_PTR, SQL_ATTR_APP_ROW_DESC, SQL_DESC_ARRAY_STATUS_PTR, SQLUSMALLINT *);
            CASE_SET_IN_DESC(SQL_ATTR_ROW_STATUS_PTR, SQL_ATTR_IMP_ROW_DESC, SQL_DESC_ARRAY_STATUS_PTR, SQLUSMALLINT *);
            CASE_SET_IN_DESC(SQL_ATTR_ROWS_FETCHED_PTR, SQL_ATTR_IMP_ROW_DESC, SQL_DESC_ROWS_PROCESSED_PTR, SQLULEN *);

#undef CASE_SET_IN_DESC

            case SQL_ATTR_NOSCAN:
                statement.setAttr(SQL_ATTR_NOSCAN, value);
                return SQL_SUCCESS;

            case SQL_ATTR_METADATA_ID:
                statement.setAttr(SQL_ATTR_METADATA_ID, value);
                return SQL_SUCCESS;

            case SQL_ATTR_APP_ROW_DESC:
            case SQL_ATTR_APP_PARAM_DESC:
            case SQL_ATTR_IMP_ROW_DESC:
            case SQL_ATTR_IMP_PARAM_DESC:
                return setDescriptorHandle(statement, attribute, reinterpret_cast<SQLHANDLE>(value));

            case SQL_ATTR_CURSOR_SCROLLABLE:
            case SQL_ATTR_CURSOR_SENSITIVITY:
            case SQL_ATTR_ASYNC_ENABLE:
            case SQL_ATTR_CONCURRENCY:
            case SQL_ATTR_CURSOR_TYPE: /// Libreoffice Base
            case SQL_ATTR_ENABLE_AUTO_IPD:
            case SQL_ATTR_FETCH_BOOKMARK_PTR:
            case SQL_ATTR_KEYSET_SIZE:
            case SQL_ATTR_MAX_LENGTH:
            case SQL_ATTR_MAX_ROWS:
            case SQL_ATTR_QUERY_TIMEOUT:
            case SQL_ATTR_RETRIEVE_DATA:
            case SQL_ATTR_ROW_NUMBER:
            case SQL_ATTR_SIMULATE_CURSOR:
            case SQL_ATTR_USE_BOOKMARKS:
                return SQL_SUCCESS;

            default:
                LOG("SetStmtAttr: Unsupported attribute " << attribute);
                //throw std::runtime_error("Unsupported statement attribute.");
                return SQL_ERROR;
        }
    };

    return CALL_WITH_HANDLE(statement_handle, func);
}

SQLRETURN GetStmtAttr(
    SQLHSTMT statement_handle,
    SQLINTEGER attribute,
    SQLPOINTER out_value,
    SQLINTEGER out_value_max_length,
    SQLINTEGER * out_value_length
) {
    auto func = [&](Statement & statement) -> SQLRETURN {
        LOG("GetStmtAttr: " << attribute << " out_value=" << out_value << " out_value_max_length=" << out_value_max_length);

        const char * name = nullptr;

        switch (attribute) {

#define CASE_GET_FROM_DESC(STMT_ATTR, DESC_TYPE, DESC_ATTR, VALUE_TYPE) \
            case STMT_ATTR: \
                return fillOutputNumber<VALUE_TYPE>(statement.getEffectiveDescriptor(DESC_TYPE).getAttrAs<VALUE_TYPE>(DESC_ATTR), out_value, SQLINTEGER{0}/* out_value_max_length */, out_value_length);

            CASE_GET_FROM_DESC(SQL_ATTR_PARAM_BIND_OFFSET_PTR, SQL_ATTR_APP_PARAM_DESC, SQL_DESC_BIND_OFFSET_PTR, SQLULEN *);
            CASE_GET_FROM_DESC(SQL_ATTR_PARAM_BIND_TYPE, SQL_ATTR_APP_PARAM_DESC, SQL_DESC_BIND_TYPE, SQLULEN);
            CASE_GET_FROM_DESC(SQL_ATTR_PARAM_OPERATION_PTR, SQL_ATTR_APP_PARAM_DESC, SQL_DESC_ARRAY_STATUS_PTR, SQLUSMALLINT *);
            CASE_GET_FROM_DESC(SQL_ATTR_PARAM_STATUS_PTR, SQL_ATTR_IMP_PARAM_DESC, SQL_DESC_ARRAY_STATUS_PTR, SQLUSMALLINT *);
            CASE_GET_FROM_DESC(SQL_ATTR_PARAMS_PROCESSED_PTR, SQL_ATTR_IMP_PARAM_DESC, SQL_DESC_ROWS_PROCESSED_PTR, SQLULEN *);
            CASE_GET_FROM_DESC(SQL_ATTR_PARAMSET_SIZE, SQL_ATTR_APP_PARAM_DESC, SQL_DESC_ARRAY_SIZE, SQLULEN);
            CASE_GET_FROM_DESC(SQL_ATTR_ROW_ARRAY_SIZE, SQL_ATTR_APP_ROW_DESC, SQL_DESC_ARRAY_SIZE, SQLULEN);
            CASE_GET_FROM_DESC(SQL_ATTR_ROW_BIND_OFFSET_PTR, SQL_ATTR_APP_ROW_DESC, SQL_DESC_BIND_OFFSET_PTR, SQLULEN *);
            CASE_GET_FROM_DESC(SQL_ATTR_ROW_BIND_TYPE, SQL_ATTR_APP_ROW_DESC, SQL_DESC_BIND_TYPE, SQLULEN);
            CASE_GET_FROM_DESC(SQL_ATTR_ROW_OPERATION_PTR, SQL_ATTR_APP_ROW_DESC, SQL_DESC_ARRAY_STATUS_PTR, SQLUSMALLINT *);
            CASE_GET_FROM_DESC(SQL_ATTR_ROW_STATUS_PTR, SQL_ATTR_IMP_ROW_DESC, SQL_DESC_ARRAY_STATUS_PTR, SQLUSMALLINT *);
            CASE_GET_FROM_DESC(SQL_ATTR_ROWS_FETCHED_PTR, SQL_ATTR_IMP_ROW_DESC, SQL_DESC_ROWS_PROCESSED_PTR, SQLULEN *);

#undef CASE_GET_FROM_DESC

            CASE_FALLTHROUGH(SQL_ATTR_APP_ROW_DESC)
            CASE_FALLTHROUGH(SQL_ATTR_APP_PARAM_DESC)
            CASE_FALLTHROUGH(SQL_ATTR_IMP_ROW_DESC)
            CASE_FALLTHROUGH(SQL_ATTR_IMP_PARAM_DESC)
				return fillOutputNumber<SQLHANDLE>(statement.getEffectiveDescriptor(attribute).getHandle(),
                    out_value, SQLINTEGER{0}/* out_value_max_length */, out_value_length);

            CASE_NUM(SQL_ATTR_CURSOR_SCROLLABLE, SQLULEN, SQL_NONSCROLLABLE);
            CASE_NUM(SQL_ATTR_CURSOR_SENSITIVITY, SQLULEN, SQL_INSENSITIVE);
            CASE_NUM(SQL_ATTR_ASYNC_ENABLE, SQLULEN, SQL_ASYNC_ENABLE_OFF);
            CASE_NUM(SQL_ATTR_CONCURRENCY, SQLULEN, SQL_CONCUR_READ_ONLY);
            CASE_NUM(SQL_ATTR_CURSOR_TYPE, SQLULEN, SQL_CURSOR_FORWARD_ONLY);
            CASE_NUM(SQL_ATTR_ENABLE_AUTO_IPD, SQLULEN, SQL_FALSE);
            CASE_NUM(SQL_ATTR_MAX_LENGTH, SQLULEN, 0);
            CASE_NUM(SQL_ATTR_MAX_ROWS, SQLULEN, 0);

            CASE_FALLTHROUGH(SQL_ATTR_METADATA_ID)
                return fillOutputNumber<SQLULEN>(
                    statement.getAttrAs<SQLULEN>(
                        SQL_ATTR_METADATA_ID,
                        statement.getParent().getAttrAs<SQLUINTEGER>(SQL_ATTR_METADATA_ID, SQL_FALSE)
                    ),
                    out_value, SQLINTEGER{0}/* out_value_max_length */, out_value_length
                );

            CASE_FALLTHROUGH(SQL_ATTR_NOSCAN)
                return fillOutputNumber<SQLULEN>(
                    statement.getAttrAs<SQLULEN>(SQL_ATTR_NOSCAN, SQL_NOSCAN_OFF),
                    out_value, SQLINTEGER{0}/* out_value_max_length */, out_value_length
                );

            CASE_NUM(SQL_ATTR_QUERY_TIMEOUT, SQLULEN, 0);
            CASE_NUM(SQL_ATTR_RETRIEVE_DATA, SQLULEN, SQL_RD_ON);
            CASE_NUM(SQL_ATTR_ROW_NUMBER, SQLULEN, statement.getCurrentRowNum());
            CASE_NUM(SQL_ATTR_USE_BOOKMARKS, SQLULEN, SQL_UB_OFF);

            case SQL_ATTR_FETCH_BOOKMARK_PTR:
            case SQL_ATTR_KEYSET_SIZE:
            case SQL_ATTR_SIMULATE_CURSOR:
            default:
                LOG("GetStmtAttr: Unsupported attribute " << attribute);
                //throw std::runtime_error("Unsupported statement attribute. " + std::to_string(attribute));
                return SQL_ERROR;
        }

        return SQL_SUCCESS;
    };

    return CALL_WITH_HANDLE(statement_handle, func);
}

} } // namespace impl


extern "C" {

SQLRETURN SQL_API SQLSetEnvAttr(SQLHENV handle, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER value_length) {
    return impl::SetEnvAttr(handle, attribute, value, value_length);
}

SQLRETURN SQL_API FUNCTION_MAYBE_W(SQLSetConnectAttr)(SQLHENV handle, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER value_length) {
    return impl::SetConnectAttr(handle, attribute, value, value_length);
}

SQLRETURN SQL_API FUNCTION_MAYBE_W(SQLSetStmtAttr)(SQLHENV handle, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER value_length) {
    return impl::SetStmtAttr(handle, attribute, value, value_length);
}

SQLRETURN SQL_API SQLGetEnvAttr(
    SQLHSTMT handle, SQLINTEGER attribute, SQLPOINTER out_value, SQLINTEGER out_value_max_length, SQLINTEGER * out_value_length) {
    return impl::GetEnvAttr(handle, attribute, out_value, out_value_max_length, out_value_length);
}

SQLRETURN SQL_API FUNCTION_MAYBE_W(SQLGetConnectAttr)(
    SQLHSTMT handle, SQLINTEGER attribute, SQLPOINTER out_value, SQLINTEGER out_value_max_length, SQLINTEGER * out_value_length) {
    return impl::GetConnectAttr(handle, attribute, out_value, out_value_max_length, out_value_length);
}

SQLRETURN SQL_API FUNCTION_MAYBE_W(SQLGetStmtAttr)(
    SQLHSTMT handle, SQLINTEGER attribute, SQLPOINTER out_value, SQLINTEGER out_value_max_length, SQLINTEGER * out_value_length) {
    return impl::GetStmtAttr(handle, attribute, out_value, out_value_max_length, out_value_length);
}

} // extern "C"
