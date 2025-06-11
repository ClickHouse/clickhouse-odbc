#include "driver/api/impl/impl.h"
#include "driver/utils/utils.h"
#include "driver/driver.h"
#include "driver/environment.h"
#include "driver/connection.h"
#include "driver/descriptor.h"
#include "driver/statement.h"

#include <Poco/Net/HTTPClientSession.h>

#include <exception>
#include <type_traits>
#include <new>

namespace impl {

SQLRETURN allocEnv(
    SQLHENV * out_environment_handle
) noexcept {
    return CALL([&] () {
        if (nullptr == out_environment_handle)
            return SQL_INVALID_HANDLE;

        *out_environment_handle = Driver::getInstance().allocateChild<Environment>().getHandle();
        return SQL_SUCCESS;
    });
}

SQLRETURN allocConnect(
    SQLHENV environment_handle,
    SQLHDBC * out_connection_handle
) noexcept {
    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_ENV, environment_handle, [&] (Environment & environment) {
        if (nullptr == out_connection_handle)
            return SQL_INVALID_HANDLE;

        *out_connection_handle = environment.allocateChild<Connection>().getHandle();
        return SQL_SUCCESS;
    });
}

SQLRETURN allocStmt(
    SQLHDBC connection_handle,
    SQLHSTMT * out_statement_handle
) noexcept {
    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DBC, connection_handle, [&] (Connection & connection) {
        if (nullptr == out_statement_handle)
            return SQL_INVALID_HANDLE;

        *out_statement_handle = connection.allocateChild<Statement>().getHandle();
        return SQL_SUCCESS;
    });
}

SQLRETURN allocDesc(
    SQLHDBC connection_handle,
    SQLHDESC * out_descriptor_handle
) noexcept {
    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DBC, connection_handle, [&] (Connection & connection) {
        if (nullptr == out_descriptor_handle)
            return SQL_INVALID_HANDLE;

        auto & descriptor = connection.allocateChild<Descriptor>();
        connection.initAsAD(descriptor, true);
        *out_descriptor_handle = descriptor.getHandle();
        return SQL_SUCCESS;
    });
}

SQLRETURN freeHandle(
    SQLHANDLE handle
) noexcept {
    return CALL_WITH_HANDLE_SKIP_DIAG(handle, [&] (auto & object) {
        if ( // Refuse to manually deallocate an automatically allocated descriptor.
            std::is_convertible<std::decay<decltype(object)> *, Descriptor *>::value &&
            object.template getAttrAs<SQLSMALLINT>(SQL_DESC_ALLOC_TYPE) != SQL_DESC_ALLOC_USER
        ) {
            return SQL_ERROR;
        }

        object.deallocateSelf();
        return SQL_SUCCESS;
    });
}

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

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_ENV, environment_handle, func);
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
                return fillOutputPOD<SQLUINTEGER>(environment.odbc_version, out_value, out_value_length);

            case SQL_ATTR_CONNECTION_POOLING:
            case SQL_ATTR_CP_MATCH:
            case SQL_ATTR_OUTPUT_NTS:
            default:
                LOG("GetEnvAttr: Unsupported attribute " << attribute);
                //throw std::runtime_error("Unsupported environment attribute.");
                return SQL_ERROR;
        }
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_ENV, environment_handle, func);
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
                connection.database = toUTF8(static_cast<PTChar*>(value), value_length / sizeof(PTChar));
                return SQL_SUCCESS;

            case SQL_ATTR_ANSI_APP:
                return SQL_ERROR;

            case CH_SQL_ATTR_DRIVERLOG: {
                if (value == reinterpret_cast<SQLPOINTER>(SQL_OPT_TRACE_ON)) {
                    connection.getDriver().setAttr(CH_SQL_ATTR_DRIVERLOG, SQL_OPT_TRACE_ON);
                }
                else if (value == reinterpret_cast<SQLPOINTER>(SQL_OPT_TRACE_OFF)) {
                    connection.getDriver().setAttr(CH_SQL_ATTR_DRIVERLOG, SQL_OPT_TRACE_OFF);
                }
                else {
                    LOG("SetConnectAttr: CH_SQL_ATTR_DRIVERLOG: Unknown value " << value);
                    return SQL_ERROR;
                }
                return SQL_SUCCESS;
            }

            case CH_SQL_ATTR_DRIVERLOGFILE:
                connection.getDriver().setAttr(CH_SQL_ATTR_DRIVERLOGFILE, toUTF8(static_cast<PTChar*>(value), value_length / sizeof(PTChar)));
                return SQL_SUCCESS;

#if defined(SQL_APPLICATION_NAME)
            case SQL_APPLICATION_NAME:
                connection.useragent = toUTF8((SQLTCHAR *)value, value_length / sizeof(SQLTCHAR));
                LOG("SetConnectAttr: SQL_APPLICATION_NAME: " << connection.useragent);
                return SQL_SUCCESS;
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

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DBC, connection_handle, func);
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
            CASE_NUM(SQL_ATTR_LOGIN_TIMEOUT, SQLUSMALLINT, connection.session ? connection.session->getTimeout().seconds() : connection.timeout);
            CASE_NUM(SQL_ATTR_TXN_ISOLATION, SQLINTEGER, SQL_TXN_SERIALIZABLE); // mssql linked server
            CASE_NUM(SQL_ATTR_AUTOCOMMIT, SQLINTEGER, SQL_AUTOCOMMIT_ON);

            case SQL_ATTR_CURRENT_CATALOG:
                return fillOutputString<PTChar>(
                    connection.database,
                    out_value, out_value_max_length, out_value_length, true
                );

            case SQL_ATTR_ANSI_APP:
                return SQL_ERROR;

            case CH_SQL_ATTR_DRIVERLOG:
                return fillOutputPOD<SQLINTEGER>(
                    (connection.getDriver().isLoggingEnabled() ? SQL_OPT_TRACE_ON : SQL_OPT_TRACE_OFF),
                    out_value, out_value_length
                );

            case CH_SQL_ATTR_DRIVERLOGFILE:
                return fillOutputString<PTChar>(
                    connection.getDriver().getAttrAs<std::string>(CH_SQL_ATTR_DRIVERLOGFILE),
                    out_value, out_value_max_length, out_value_length, true
                );

            case SQL_ATTR_METADATA_ID:
                return fillOutputPOD<SQLUINTEGER>(
                    connection.getAttrAs<SQLUINTEGER>(SQL_ATTR_METADATA_ID, SQL_FALSE),
                    out_value, out_value_length
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

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DBC, connection_handle, func);
}

namespace  {

    SQLRETURN setDescriptorHandle(
        Statement & statement,
        SQLINTEGER descriptor_type,
        SQLHANDLE descriptor_handle
    ) {
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

        auto rc = CALL_WITH_TYPED_HANDLE_SKIP_DIAG(SQL_HANDLE_DESC, descriptor_handle, func);

        if (ex)
            std::rethrow_exception(ex);

        if (rc == SQL_INVALID_HANDLE)
            throw SqlException("Invalid attribute value", "HY024");

        return rc;
    }

} // namespace

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

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, func);
}

SQLRETURN GetStmtAttr(
    SQLHSTMT statement_handle,
    SQLINTEGER attribute,
    SQLPOINTER out_value,
    SQLINTEGER out_value_max_length,
    SQLINTEGER * out_value_length
) noexcept {
    auto func = [&](Statement & statement) -> SQLRETURN {
        LOG("GetStmtAttr: " << attribute << " out_value=" << out_value << " out_value_max_length=" << out_value_max_length);

        const char * name = nullptr;

        switch (attribute) {

#define CASE_GET_FROM_DESC(STMT_ATTR, DESC_TYPE, DESC_ATTR, VALUE_TYPE) \
            case STMT_ATTR: \
                return fillOutputPOD<VALUE_TYPE>(statement.getEffectiveDescriptor(DESC_TYPE).getAttrAs<VALUE_TYPE>(DESC_ATTR), out_value, out_value_length);

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
				return fillOutputPOD<SQLHANDLE>(statement.getEffectiveDescriptor(attribute).getHandle(),
                    out_value, out_value_length);

            CASE_NUM(SQL_ATTR_CURSOR_SCROLLABLE, SQLULEN, SQL_NONSCROLLABLE);
            CASE_NUM(SQL_ATTR_CURSOR_SENSITIVITY, SQLULEN, SQL_INSENSITIVE);
            CASE_NUM(SQL_ATTR_ASYNC_ENABLE, SQLULEN, SQL_ASYNC_ENABLE_OFF);
            CASE_NUM(SQL_ATTR_CONCURRENCY, SQLULEN, SQL_CONCUR_READ_ONLY);
            CASE_NUM(SQL_ATTR_CURSOR_TYPE, SQLULEN, SQL_CURSOR_FORWARD_ONLY);
            CASE_NUM(SQL_ATTR_ENABLE_AUTO_IPD, SQLULEN, SQL_FALSE);
            CASE_NUM(SQL_ATTR_MAX_LENGTH, SQLULEN, 0);
            CASE_NUM(SQL_ATTR_MAX_ROWS, SQLULEN, 0);

            CASE_FALLTHROUGH(SQL_ATTR_METADATA_ID)
                return fillOutputPOD<SQLULEN>(
                    statement.getAttrAs<SQLULEN>(
                        SQL_ATTR_METADATA_ID,
                        statement.getParent().getAttrAs<SQLUINTEGER>(SQL_ATTR_METADATA_ID, SQL_FALSE)
                    ),
                    out_value, out_value_length
                );

            CASE_FALLTHROUGH(SQL_ATTR_NOSCAN)
                return fillOutputPOD<SQLULEN>(
                    statement.getAttrAs<SQLULEN>(SQL_ATTR_NOSCAN, SQL_NOSCAN_OFF),
                    out_value, out_value_length
                );

            case SQL_ATTR_ROW_NUMBER: {
                if (!statement.hasResultSet())
                    throw SqlException("Invalid cursor state", "24000");

                auto & result_set = statement.getResultSet();
                return fillOutputPOD<SQLULEN>(result_set.getCurrentRowPosition(), out_value, out_value_length);
            }

            CASE_NUM(SQL_ATTR_QUERY_TIMEOUT, SQLULEN, 0);
            CASE_NUM(SQL_ATTR_RETRIEVE_DATA, SQLULEN, SQL_RD_ON);
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

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, statement_handle, func);
}

SQLRETURN GetDiagRec(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLTCHAR * out_sqlstate,
    SQLINTEGER * out_native_error_code,
    SQLTCHAR * out_message,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size
) noexcept {
    auto func = [&] (auto & object) -> SQLRETURN {
        if (record_number < 1 || out_message_max_size < 0)
            return SQL_ERROR;

        if (record_number > object.getDiagStatusCount())
            return SQL_NO_DATA;

        const auto & record = object.getDiagStatus(record_number);

        if (out_sqlstate) {
            std::size_t size = 6;
            std::size_t written = 0;
            fillOutputString<PTChar>(record.template getAttrAs<std::string>(SQL_DIAG_SQLSTATE), out_sqlstate, size, &written, false);
        }

        if (out_native_error_code != nullptr) {
            *out_native_error_code = record.template getAttrAs<SQLINTEGER>(SQL_DIAG_NATIVE);
        }

        return fillOutputString<PTChar>(record.template getAttrAs<std::string>(SQL_DIAG_MESSAGE_TEXT), out_message, out_message_max_size, out_message_size, false);
    };

    return CALL_WITH_TYPED_HANDLE_SKIP_DIAG(handle_type, handle, func);
}

SQLRETURN GetDiagField(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLSMALLINT field_id,
    SQLPOINTER out_message,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size
) noexcept {
    auto func = [&] (auto & object) -> SQLRETURN {
        // Exit with error if the requested field is relevant only to statements.
        if (getObjectHandleType<std::decay_t<decltype(object)>>() != SQL_HANDLE_STMT) {
            switch (field_id) {
                case SQL_DIAG_CURSOR_ROW_COUNT:
                case SQL_DIAG_DYNAMIC_FUNCTION:
                case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
                case SQL_DIAG_ROW_COUNT:
                    return SQL_ERROR;
            }
        }

        // Ignore (adjust) record_number if the requested field is relevant only to the header.
        switch (field_id) {
            case SQL_DIAG_CURSOR_ROW_COUNT:
            case SQL_DIAG_DYNAMIC_FUNCTION:
            case SQL_DIAG_DYNAMIC_FUNCTION_CODE:
            case SQL_DIAG_NUMBER:
            case SQL_DIAG_RETURNCODE:
            case SQL_DIAG_ROW_COUNT:
                record_number = 0;
                break;
        }

        if (record_number < 0)
            return SQL_ERROR;

        if (record_number > 0 && record_number > object.getDiagStatusCount())
            return SQL_NO_DATA;

        const auto & record = object.getDiagStatus(record_number);

        switch (field_id) {

#define CASE_ATTR_NUM(NAME, TYPE) \
    case NAME: \
        return fillOutputPOD(record.template getAttrAs<TYPE>(NAME), out_message, out_message_size);

#define CASE_ATTR_STR(NAME) \
    case NAME: \
        return fillOutputString<PTChar>(record.template getAttrAs<std::string>(NAME), out_message, out_message_max_size, out_message_size, true);

            CASE_ATTR_NUM(SQL_DIAG_CURSOR_ROW_COUNT, SQLLEN);
            CASE_ATTR_STR(SQL_DIAG_DYNAMIC_FUNCTION);
            CASE_ATTR_NUM(SQL_DIAG_DYNAMIC_FUNCTION_CODE, SQLINTEGER);
            CASE_ATTR_NUM(SQL_DIAG_NUMBER, SQLINTEGER);
            CASE_ATTR_NUM(SQL_DIAG_RETURNCODE, SQLRETURN);
            CASE_ATTR_NUM(SQL_DIAG_ROW_COUNT, SQLLEN);

            CASE_ATTR_STR(SQL_DIAG_CLASS_ORIGIN);
            CASE_ATTR_NUM(SQL_DIAG_COLUMN_NUMBER, SQLINTEGER);
            CASE_ATTR_STR(SQL_DIAG_CONNECTION_NAME);
            CASE_ATTR_STR(SQL_DIAG_MESSAGE_TEXT);
            CASE_ATTR_NUM(SQL_DIAG_NATIVE, SQLINTEGER);
            CASE_ATTR_NUM(SQL_DIAG_ROW_NUMBER, SQLLEN);
            CASE_ATTR_STR(SQL_DIAG_SERVER_NAME);
            CASE_ATTR_STR(SQL_DIAG_SQLSTATE);
            CASE_ATTR_STR(SQL_DIAG_SUBCLASS_ORIGIN);

#undef CASE_ATTR_NUM
#undef CASE_ATTR_STR

        }

        return SQL_ERROR;
    };

    return CALL_WITH_TYPED_HANDLE_SKIP_DIAG(handle_type, handle, func);
}

SQLRETURN BindCol(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   ColumnNumber,
    SQLSMALLINT    TargetType,
    SQLPOINTER     TargetValuePtr,
    SQLLEN         BufferLength,
    SQLLEN *       StrLen_or_Ind
) noexcept {
    auto func = [&] (Statement & statement) {
        if (ColumnNumber < 1)
            throw SqlException("Invalid descriptor index", "07009");

        auto & ard_desc = statement.getEffectiveDescriptor(SQL_ATTR_APP_ROW_DESC);
        const auto ard_record_count = ard_desc.getRecordCount();

        // If TargetValuePtr and StrLen_or_Ind pointers are both NULL, then unbind the column,
        // and if the column number is [greater than or] equal to the highest bound column number,
        // then adjust SQL_DESC_COUNT to reflect that the highest bound column has chaged.
        if (
            TargetValuePtr == nullptr &&
            StrLen_or_Ind == nullptr &&
            ColumnNumber >= ard_record_count
        ) {
            auto nextHighestBoundColumnNumber = std::min<std::size_t>(ard_record_count, ColumnNumber - 1);

            while (nextHighestBoundColumnNumber > 0) {
                auto & ard_record = ard_desc.getRecord(nextHighestBoundColumnNumber, SQL_ATTR_APP_ROW_DESC);

                if (
                    ard_record.getAttrAs<SQLPOINTER>(SQL_DESC_DATA_PTR, 0) != nullptr ||
                    ard_record.getAttrAs<SQLLEN *>(SQL_DESC_OCTET_LENGTH_PTR, 0) != nullptr ||
                    ard_record.getAttrAs<SQLLEN *>(SQL_DESC_INDICATOR_PTR, 0) != nullptr
                ) {
                    break;
                }

                --nextHighestBoundColumnNumber;
            }

            ard_desc.setAttr(SQL_DESC_COUNT, nextHighestBoundColumnNumber);
            return SQL_SUCCESS;
        }

        auto & ard_record = ard_desc.getRecord(ColumnNumber, SQL_ATTR_APP_ROW_DESC);

        try {
            // This will trigger automatic (re)setting of SQL_DESC_TYPE and SQL_DESC_DATETIME_INTERVAL_CODE,
            // and resetting of SQL_DESC_DATA_PTR.
            ard_record.setAttr(SQL_DESC_CONCISE_TYPE, TargetType);

            switch (TargetType) {
                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                case SQL_C_BINARY:
//              case SQL_C_VARBOOKMARK:
                    if (BufferLength < 0)
                        throw SqlException("Invalid string or buffer length", "HY090");

                    ard_record.setAttr(SQL_DESC_LENGTH, BufferLength);
                    ard_record.setAttr(SQL_DESC_OCTET_LENGTH, BufferLength);
                    break;

                default:
                    ard_record.setAttr(SQL_DESC_OCTET_LENGTH, getCTypeOctetLength(TargetType));
                    break;
            }

            ard_record.setAttr(SQL_DESC_OCTET_LENGTH_PTR, StrLen_or_Ind);
            ard_record.setAttr(SQL_DESC_INDICATOR_PTR, StrLen_or_Ind);
            ard_record.setAttr(SQL_DESC_DATA_PTR, TargetValuePtr);
        }
        catch (...) {
            ard_desc.setAttr(SQL_DESC_COUNT, ard_record_count);
            throw;
        }

        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, StatementHandle, func);
}

SQLRETURN BindParameter(
    SQLHSTMT        handle,
    SQLUSMALLINT    parameter_number,
    SQLSMALLINT     input_output_type,
    SQLSMALLINT     value_type,
    SQLSMALLINT     parameter_type,
    SQLULEN         column_size,
    SQLSMALLINT     decimal_digits,
    SQLPOINTER      parameter_value_ptr,
    SQLLEN          buffer_length,
    SQLLEN *        StrLen_or_IndPtr
) noexcept {
    auto func = [&] (Statement & statement) {
        if (parameter_number < 1)
            throw SqlException("Invalid descriptor index", "07009");

        auto & apd_desc = statement.getEffectiveDescriptor(SQL_ATTR_APP_PARAM_DESC);
        auto & ipd_desc = statement.getEffectiveDescriptor(SQL_ATTR_IMP_PARAM_DESC);

        const auto apd_record_count = apd_desc.getRecordCount();
        const auto ipd_record_count = ipd_desc.getRecordCount();

        // If parameter_value_ptr and StrLen_or_IndPtr pointers are both NULL, then unbind the parameter,
        // and if the parameter number is [greater than or] equal to the highest bound parameter number,
        // then adjust SQL_DESC_COUNT to reflect that the highest bound parameter has chaged.
        if (
            parameter_value_ptr == nullptr &&
            StrLen_or_IndPtr == nullptr &&
            parameter_number >= apd_record_count
        ) {
            auto nextHighestBoundParameterNumber = std::min<std::size_t>(apd_record_count, parameter_number - 1);

            while (nextHighestBoundParameterNumber > 0) {
                auto & apd_record = apd_desc.getRecord(nextHighestBoundParameterNumber, SQL_ATTR_APP_PARAM_DESC);

                if (
                    apd_record.getAttrAs<SQLPOINTER>(SQL_DESC_DATA_PTR, 0) != nullptr ||
                    apd_record.getAttrAs<SQLLEN *>(SQL_DESC_OCTET_LENGTH_PTR, 0) != nullptr ||
                    apd_record.getAttrAs<SQLLEN *>(SQL_DESC_INDICATOR_PTR, 0) != nullptr
                ) {
                    break;
                }

                --nextHighestBoundParameterNumber;
            }

            apd_desc.setAttr(SQL_DESC_COUNT, nextHighestBoundParameterNumber);
            return SQL_SUCCESS;
        }

        auto & apd_record = apd_desc.getRecord(parameter_number, SQL_ATTR_APP_PARAM_DESC);
        auto & ipd_record = ipd_desc.getRecord(parameter_number, SQL_ATTR_IMP_PARAM_DESC);

        try {
            ipd_record.setAttr(SQL_DESC_PARAMETER_TYPE, input_output_type);

            // These two will trigger automatic (re)setting of SQL_DESC_TYPE and SQL_DESC_DATETIME_INTERVAL_CODE,
            // and resetting of SQL_DESC_DATA_PTR.
            apd_record.setAttr(SQL_DESC_CONCISE_TYPE,
                (value_type == SQL_C_DEFAULT ? convertSQLTypeToCType(parameter_type) : value_type)
            );
            ipd_record.setAttr(SQL_DESC_CONCISE_TYPE, parameter_type);

            switch (parameter_type) {
                case SQL_CHAR:
                case SQL_WCHAR:
                case SQL_VARCHAR:
                case SQL_WVARCHAR:
                case SQL_LONGVARCHAR:
                case SQL_WLONGVARCHAR:
                case SQL_BINARY:
                case SQL_VARBINARY:
                case SQL_LONGVARBINARY:
                case SQL_TYPE_DATE:
                case SQL_TYPE_TIME:
                case SQL_TYPE_TIMESTAMP:
                case SQL_INTERVAL_MONTH:
                case SQL_INTERVAL_YEAR:
                case SQL_INTERVAL_YEAR_TO_MONTH:
                case SQL_INTERVAL_DAY:
                case SQL_INTERVAL_HOUR:
                case SQL_INTERVAL_MINUTE:
                case SQL_INTERVAL_SECOND:
                case SQL_INTERVAL_DAY_TO_HOUR:
                case SQL_INTERVAL_DAY_TO_MINUTE:
                case SQL_INTERVAL_DAY_TO_SECOND:
                case SQL_INTERVAL_HOUR_TO_MINUTE:
                case SQL_INTERVAL_HOUR_TO_SECOND:
                case SQL_INTERVAL_MINUTE_TO_SECOND:
                    ipd_record.setAttr(SQL_DESC_LENGTH, column_size);
                    break;

                case SQL_DECIMAL:
                case SQL_NUMERIC:
                case SQL_FLOAT:
                case SQL_REAL:
                case SQL_DOUBLE:
                    ipd_record.setAttr(SQL_DESC_PRECISION, column_size);
                    break;
            }

            switch (parameter_type) {
                case SQL_TYPE_TIME:
                case SQL_TYPE_TIMESTAMP:
                case SQL_INTERVAL_SECOND:
                case SQL_INTERVAL_DAY_TO_SECOND:
                case SQL_INTERVAL_HOUR_TO_SECOND:
                case SQL_INTERVAL_MINUTE_TO_SECOND:
                    ipd_record.setAttr(SQL_DESC_PRECISION, decimal_digits);
                    break;

                case SQL_NUMERIC:
                case SQL_DECIMAL:
                    ipd_record.setAttr(SQL_DESC_SCALE, decimal_digits);
                    break;
            }

            const auto parameter_c_type = convertSQLTypeToCType(parameter_type);
            switch (parameter_c_type) {
                case SQL_C_CHAR:
                case SQL_C_WCHAR:
                case SQL_C_BINARY:
//              case SQL_C_VARBOOKMARK:
                    if (buffer_length < 0)
                        throw SqlException("Invalid string or buffer length", "HY090");

                    apd_record.setAttr(SQL_DESC_OCTET_LENGTH, buffer_length);
                    break;

                default:
                    apd_record.setAttr(SQL_DESC_OCTET_LENGTH, getCTypeOctetLength(parameter_c_type));
                    break;
            }

            apd_record.setAttr(SQL_DESC_OCTET_LENGTH_PTR, StrLen_or_IndPtr);
            apd_record.setAttr(SQL_DESC_INDICATOR_PTR, StrLen_or_IndPtr);
            apd_record.setAttr(SQL_DESC_DATA_PTR, parameter_value_ptr);
        }
        catch (...) {
            apd_desc.setAttr(SQL_DESC_COUNT, apd_record_count);
            ipd_desc.setAttr(SQL_DESC_COUNT, ipd_record_count);

            throw;
        }

        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, handle, func);
}

SQLRETURN NumParams(
    SQLHSTMT        handle,
    SQLSMALLINT *   out_parameter_count
) noexcept {
    auto func = [&] (Statement & statement) {
        auto & ipd_desc = statement.getEffectiveDescriptor(SQL_ATTR_IMP_PARAM_DESC);
        const auto ipd_record_count = ipd_desc.getRecordCount();

        *out_parameter_count = ipd_record_count; // TODO: ...or statement.parameters.size()?

        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, handle, func);
}

SQLRETURN DescribeParam(
    SQLHSTMT        handle,
    SQLUSMALLINT    parameter_number,
    SQLSMALLINT *   out_data_type_ptr,
    SQLULEN *       out_parameter_size_ptr,
    SQLSMALLINT *   out_decimal_digits_ptr,
    SQLSMALLINT *   out_nullable_ptr
) noexcept {
    auto func = [&] (Statement & statement) {
        auto & ipd_desc = statement.getEffectiveDescriptor(SQL_ATTR_IMP_PARAM_DESC);

        if (parameter_number < 0 || parameter_number > ipd_desc.getRecordCount())
            throw SqlException("Invalid descriptor index", "07009");

        auto & ipd_record = ipd_desc.getRecord(parameter_number, SQL_ATTR_IMP_PARAM_DESC);

        *out_data_type_ptr = ipd_record.getAttrAs<SQLSMALLINT>(SQL_DESC_CONCISE_TYPE);
        *out_nullable_ptr = ipd_record.getAttrAs<SQLSMALLINT>(SQL_DESC_NULLABLE, SQL_NULLABLE_UNKNOWN);

        if (ipd_record.hasColumnSize())
            *out_parameter_size_ptr = ipd_record.getColumnSize();

        if (ipd_record.hasDecimalDigits())
            *out_decimal_digits_ptr = ipd_record.getDecimalDigits();

        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, handle, func);
}

SQLRETURN GetDescField(
    SQLHDESC        DescriptorHandle,
    SQLSMALLINT     RecNumber,
    SQLSMALLINT     FieldIdentifier,
    SQLPOINTER      ValuePtr,
    SQLINTEGER      BufferLength,
    SQLINTEGER *    StringLengthPtr
) noexcept {
    auto func = [&] (Descriptor & descriptor) -> SQLRETURN {

        // Process header fields first, withour running checks on record number.
        switch (FieldIdentifier) {

#define CASE_FIELD_NUM(NAME, TYPE) \
            case NAME: return fillOutputPOD<TYPE>(descriptor.getAttrAs<TYPE>(NAME), ValuePtr, StringLengthPtr);

#define CASE_FIELD_NUM_DEF(NAME, TYPE, DEFAULT) \
            case NAME: return fillOutputPOD<TYPE>(descriptor.getAttrAs<TYPE>(NAME, DEFAULT), ValuePtr, StringLengthPtr);

            CASE_FIELD_NUM     ( SQL_DESC_ALLOC_TYPE,         SQLSMALLINT                          );
            CASE_FIELD_NUM_DEF ( SQL_DESC_ARRAY_SIZE,         SQLULEN,       1                     );
            CASE_FIELD_NUM     ( SQL_DESC_ARRAY_STATUS_PTR,   SQLUSMALLINT *                       );
            CASE_FIELD_NUM     ( SQL_DESC_BIND_OFFSET_PTR,    SQLLEN *                             );
            CASE_FIELD_NUM_DEF ( SQL_DESC_BIND_TYPE,          SQLUINTEGER,   SQL_BIND_TYPE_DEFAULT );
            CASE_FIELD_NUM     ( SQL_DESC_COUNT,              SQLSMALLINT                          );
            CASE_FIELD_NUM     ( SQL_DESC_ROWS_PROCESSED_PTR, SQLULEN *                            );

#undef CASE_FIELD_NUM_DEF
#undef CASE_FIELD_NUM

        }

        if (RecNumber < 0)
            throw SqlException("Invalid descriptor index", "07009");

        if (RecNumber > descriptor.getRecordCount())
            return SQL_NO_DATA;

        auto & record = descriptor.getRecord(RecNumber, SQL_ATTR_APP_ROW_DESC); // TODO: descriptor type?

        switch (FieldIdentifier) {

#define CASE_FIELD_NUM(NAME, TYPE) \
            case NAME: return fillOutputPOD<TYPE>(record.getAttrAs<TYPE>(NAME), ValuePtr, StringLengthPtr);

#define CASE_FIELD_NUM_DEF(NAME, TYPE, DEFAULT) \
            case NAME: return fillOutputPOD<TYPE>(record.getAttrAs<TYPE>(NAME, DEFAULT), ValuePtr, StringLengthPtr);

#define CASE_FIELD_STR(NAME) \
            case NAME: return fillOutputString<PTChar>(record.getAttrAs<std::string>(NAME), ValuePtr, BufferLength, StringLengthPtr, true);

            CASE_FIELD_NUM     ( SQL_DESC_AUTO_UNIQUE_VALUE,           SQLINTEGER                              );
            CASE_FIELD_STR     ( SQL_DESC_BASE_COLUMN_NAME                                                     );
            CASE_FIELD_STR     ( SQL_DESC_BASE_TABLE_NAME                                                      );
            CASE_FIELD_NUM     ( SQL_DESC_CASE_SENSITIVE,              SQLINTEGER                              );
            CASE_FIELD_STR     ( SQL_DESC_CATALOG_NAME                                                         );
            CASE_FIELD_NUM     ( SQL_DESC_CONCISE_TYPE,                SQLSMALLINT                             );
            CASE_FIELD_NUM     ( SQL_DESC_DATA_PTR,                    SQLPOINTER                              );
            CASE_FIELD_NUM     ( SQL_DESC_DATETIME_INTERVAL_CODE,      SQLSMALLINT                             );
            CASE_FIELD_NUM     ( SQL_DESC_DATETIME_INTERVAL_PRECISION, SQLINTEGER                              );
            CASE_FIELD_NUM     ( SQL_DESC_DISPLAY_SIZE,                SQLINTEGER                              );
            CASE_FIELD_NUM     ( SQL_DESC_FIXED_PREC_SCALE,            SQLSMALLINT                             );
            CASE_FIELD_NUM     ( SQL_DESC_INDICATOR_PTR,               SQLLEN *                                );
            CASE_FIELD_STR     ( SQL_DESC_LABEL                                                                );
            CASE_FIELD_NUM     ( SQL_DESC_LENGTH,                      SQLULEN                                 );
            CASE_FIELD_STR     ( SQL_DESC_LITERAL_PREFIX                                                       );
            CASE_FIELD_STR     ( SQL_DESC_LITERAL_SUFFIX                                                       );
            CASE_FIELD_STR     ( SQL_DESC_LOCAL_TYPE_NAME                                                      );
            CASE_FIELD_STR     ( SQL_DESC_NAME                                                                 );
            CASE_FIELD_NUM     ( SQL_DESC_NULLABLE,                    SQLSMALLINT                             );
            CASE_FIELD_NUM     ( SQL_DESC_NUM_PREC_RADIX,              SQLINTEGER                              );
            CASE_FIELD_NUM     ( SQL_DESC_OCTET_LENGTH,                SQLLEN                                  );
            CASE_FIELD_NUM     ( SQL_DESC_OCTET_LENGTH_PTR,            SQLLEN *                                );
            CASE_FIELD_NUM     ( SQL_DESC_PARAMETER_TYPE,              SQLSMALLINT                             );
            CASE_FIELD_NUM     ( SQL_DESC_PRECISION,                   SQLSMALLINT                             );
            CASE_FIELD_NUM     ( SQL_DESC_ROWVER,                      SQLSMALLINT                             );
            CASE_FIELD_NUM     ( SQL_DESC_SCALE,                       SQLSMALLINT                             );
            CASE_FIELD_STR     ( SQL_DESC_SCHEMA_NAME                                                          );
            CASE_FIELD_NUM_DEF ( SQL_DESC_SEARCHABLE,                  SQLSMALLINT, SQL_PRED_SEARCHABLE        );
            CASE_FIELD_STR     ( SQL_DESC_TABLE_NAME                                                           );
            CASE_FIELD_NUM     ( SQL_DESC_TYPE,                        SQLSMALLINT                             );
            CASE_FIELD_STR     ( SQL_DESC_TYPE_NAME                                                            );
            CASE_FIELD_NUM     ( SQL_DESC_UNNAMED,                     SQLSMALLINT                             );
            CASE_FIELD_NUM     ( SQL_DESC_UNSIGNED,                    SQLSMALLINT                             );
            CASE_FIELD_NUM_DEF ( SQL_DESC_UPDATABLE,                   SQLSMALLINT, SQL_ATTR_READWRITE_UNKNOWN );

#undef CASE_FIELD_STR
#undef CASE_FIELD_NUM_DEF
#undef CASE_FIELD_NUM

        }

        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DESC, DescriptorHandle, func);
}

SQLRETURN GetDescRec(
    SQLHDESC        DescriptorHandle,
    SQLSMALLINT     RecNumber,
    SQLTCHAR *      Name,
    SQLSMALLINT     BufferLength,
    SQLSMALLINT *   StringLengthPtr,
    SQLSMALLINT *   TypePtr,
    SQLSMALLINT *   SubTypePtr,
    SQLLEN *        LengthPtr,
    SQLSMALLINT *   PrecisionPtr,
    SQLSMALLINT *   ScalePtr,
    SQLSMALLINT *   NullablePtr
) noexcept {
    auto func = [&] (Descriptor & descriptor) -> SQLRETURN {
        if (RecNumber < 0)
            throw SqlException("Invalid descriptor index", "07009");

        if (RecNumber > descriptor.getRecordCount())
            return SQL_NO_DATA;

        auto & record = descriptor.getRecord(RecNumber, SQL_ATTR_APP_ROW_DESC); // TODO: descriptor type?

        auto has_type = record.hasAttrInteger(SQL_DESC_TYPE);
        auto type = record.getAttrAs<SQLSMALLINT>(SQL_DESC_TYPE);

        if (TypePtr && has_type)
            *TypePtr = type;

        if (SubTypePtr && has_type && isVerboseType(type) && record.hasAttrInteger(SQL_DESC_DATETIME_INTERVAL_CODE))
            *SubTypePtr = record.getAttrAs<SQLSMALLINT>(SQL_DESC_DATETIME_INTERVAL_CODE);

        if (LengthPtr && record.hasAttrInteger(SQL_DESC_OCTET_LENGTH))
            *LengthPtr = record.getAttrAs<SQLLEN>(SQL_DESC_OCTET_LENGTH);

        if (PrecisionPtr && record.hasAttrInteger(SQL_DESC_PRECISION))
            *PrecisionPtr = record.getAttrAs<SQLSMALLINT>(SQL_DESC_PRECISION);

        if (ScalePtr && record.hasAttrInteger(SQL_DESC_SCALE))
            *ScalePtr = record.getAttrAs<SQLSMALLINT>(SQL_DESC_SCALE);

        if (NullablePtr && record.hasAttrInteger(SQL_DESC_NULLABLE))
            *NullablePtr = record.getAttrAs<SQLSMALLINT>(SQL_DESC_NULLABLE);

        return fillOutputString<PTChar>(record.getAttrAs<std::string>(SQL_DESC_NAME), Name, BufferLength, StringLengthPtr, false);
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DESC, DescriptorHandle, func);
}

SQLRETURN SetDescField(
    SQLHDESC      DescriptorHandle,
    SQLSMALLINT   RecNumber,
    SQLSMALLINT   FieldIdentifier,
    SQLPOINTER    ValuePtr,
    SQLINTEGER    BufferLength
) noexcept {
    auto func = [&] (Descriptor & descriptor) -> SQLRETURN {

        // Process header fields first, withour running checks on record number.
        switch (FieldIdentifier) {

#define CASE_FIELD_NUM(NAME, TYPE) \
            case NAME: { descriptor.setAttr(NAME, (TYPE)(reinterpret_cast<std::uintptr_t>(ValuePtr))); return SQL_SUCCESS; }

            CASE_FIELD_NUM ( SQL_DESC_ALLOC_TYPE,         SQLSMALLINT    );
            CASE_FIELD_NUM ( SQL_DESC_ARRAY_SIZE,         SQLULEN        );
            CASE_FIELD_NUM ( SQL_DESC_ARRAY_STATUS_PTR,   SQLUSMALLINT * );
            CASE_FIELD_NUM ( SQL_DESC_BIND_OFFSET_PTR,    SQLLEN *       );
            CASE_FIELD_NUM ( SQL_DESC_BIND_TYPE,          SQLUINTEGER    );
            CASE_FIELD_NUM ( SQL_DESC_COUNT,              SQLSMALLINT    );
            CASE_FIELD_NUM ( SQL_DESC_ROWS_PROCESSED_PTR, SQLULEN *      );

#undef CASE_FIELD_NUM

        }

        if (RecNumber < 0)
            throw SqlException("Invalid descriptor index", "07009");

        auto & record = descriptor.getRecord(RecNumber, SQL_ATTR_APP_ROW_DESC); // TODO: descriptor type?

        switch (FieldIdentifier) {

#define CASE_FIELD_NUM(NAME, TYPE) \
            case NAME: { record.setAttr(NAME, (TYPE)(reinterpret_cast<std::uintptr_t>(ValuePtr))); return SQL_SUCCESS; }

#define CASE_FIELD_STR(NAME) \
            case NAME: { \
                std::string value; \
                if (ValuePtr) { \
                    if (BufferLength > 0) \
                        value = std::string{static_cast<char *>(ValuePtr), static_cast<std::string::size_type>(BufferLength)}; \
                    else if (BufferLength == SQL_NTS) \
                        value = std::string{static_cast<char *>(ValuePtr)}; \
                } \
                record.setAttr(NAME, value); \
                return SQL_SUCCESS; \
            }

            CASE_FIELD_NUM ( SQL_DESC_AUTO_UNIQUE_VALUE,           SQLINTEGER  );
            CASE_FIELD_STR ( SQL_DESC_BASE_COLUMN_NAME                         );
            CASE_FIELD_STR ( SQL_DESC_BASE_TABLE_NAME                          );
            CASE_FIELD_NUM ( SQL_DESC_CASE_SENSITIVE,              SQLINTEGER  );
            CASE_FIELD_STR ( SQL_DESC_CATALOG_NAME                             );
            CASE_FIELD_NUM ( SQL_DESC_CONCISE_TYPE,                SQLSMALLINT );
            CASE_FIELD_NUM ( SQL_DESC_DATA_PTR,                    SQLPOINTER  );
            CASE_FIELD_NUM ( SQL_DESC_DATETIME_INTERVAL_CODE,      SQLSMALLINT );
            CASE_FIELD_NUM ( SQL_DESC_DATETIME_INTERVAL_PRECISION, SQLINTEGER  );
            CASE_FIELD_NUM ( SQL_DESC_DISPLAY_SIZE,                SQLINTEGER  );
            CASE_FIELD_NUM ( SQL_DESC_FIXED_PREC_SCALE,            SQLSMALLINT );
            CASE_FIELD_NUM ( SQL_DESC_INDICATOR_PTR,               SQLLEN *    );
            CASE_FIELD_STR ( SQL_DESC_LABEL                                    );
            CASE_FIELD_NUM ( SQL_DESC_LENGTH,                      SQLULEN     );
            CASE_FIELD_STR ( SQL_DESC_LITERAL_PREFIX                           );
            CASE_FIELD_STR ( SQL_DESC_LITERAL_SUFFIX                           );
            CASE_FIELD_STR ( SQL_DESC_LOCAL_TYPE_NAME                          );
            CASE_FIELD_STR ( SQL_DESC_NAME                                     );
            CASE_FIELD_NUM ( SQL_DESC_NULLABLE,                    SQLSMALLINT );
            CASE_FIELD_NUM ( SQL_DESC_NUM_PREC_RADIX,              SQLINTEGER  );
            CASE_FIELD_NUM ( SQL_DESC_OCTET_LENGTH,                SQLLEN      );
            CASE_FIELD_NUM ( SQL_DESC_OCTET_LENGTH_PTR,            SQLLEN *    );
            CASE_FIELD_NUM ( SQL_DESC_PARAMETER_TYPE,              SQLSMALLINT );
            CASE_FIELD_NUM ( SQL_DESC_PRECISION,                   SQLSMALLINT );
            CASE_FIELD_NUM ( SQL_DESC_ROWVER,                      SQLSMALLINT );
            CASE_FIELD_NUM ( SQL_DESC_SCALE,                       SQLSMALLINT );
            CASE_FIELD_STR ( SQL_DESC_SCHEMA_NAME                              );
            CASE_FIELD_NUM ( SQL_DESC_SEARCHABLE,                  SQLSMALLINT );
            CASE_FIELD_STR ( SQL_DESC_TABLE_NAME                               );
            CASE_FIELD_NUM ( SQL_DESC_TYPE,                        SQLSMALLINT );
            CASE_FIELD_STR ( SQL_DESC_TYPE_NAME                                );
            CASE_FIELD_NUM ( SQL_DESC_UNNAMED,                     SQLSMALLINT );
            CASE_FIELD_NUM ( SQL_DESC_UNSIGNED,                    SQLSMALLINT );
            CASE_FIELD_NUM ( SQL_DESC_UPDATABLE,                   SQLSMALLINT );

#undef CASE_FIELD_STR
#undef CASE_FIELD_NUM

        }

        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DESC, DescriptorHandle, func);
}

SQLRETURN SetDescRec(
    SQLHDESC      DescriptorHandle,
    SQLSMALLINT   RecNumber,
    SQLSMALLINT   Type,
    SQLSMALLINT   SubType,
    SQLLEN        Length,
    SQLSMALLINT   Precision,
    SQLSMALLINT   Scale,
    SQLPOINTER    DataPtr,
    SQLLEN *      StringLengthPtr,
    SQLLEN *      IndicatorPtr
) noexcept {
    auto func = [&] (Descriptor & descriptor) {
        auto & record = descriptor.getRecord(RecNumber, SQL_ATTR_APP_ROW_DESC); // TODO: descriptor type?

        record.setAttr(SQL_DESC_TYPE, Type);

        if (isVerboseType(Type))
            record.setAttr(SQL_DESC_DATETIME_INTERVAL_CODE, SubType);

        record.setAttr(SQL_DESC_OCTET_LENGTH, Length);
        record.setAttr(SQL_DESC_PRECISION, Precision);
        record.setAttr(SQL_DESC_SCALE, Scale);
        record.setAttr(SQL_DESC_OCTET_LENGTH_PTR, StringLengthPtr);
        record.setAttr(SQL_DESC_INDICATOR_PTR, IndicatorPtr);
        record.setAttr(SQL_DESC_DATA_PTR, DataPtr);

        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DESC, DescriptorHandle, func);
}

SQLRETURN CopyDesc(
    SQLHDESC     SourceDescHandle,
    SQLHDESC     TargetDescHandle
) noexcept {
    auto func = [&] (Descriptor & target) {
        // We don't want to modify the diagnostics info of the source descriptor,
        // but we want to forward it to the target descriptor context unchanged,
        // so we are going to intercept exceptions and process them outside
        // the target descriptor dispatch closure.
        std::exception_ptr ex;

        auto func = [&] (Descriptor & source) {
            try {
                target = source;

                return SQL_SUCCESS;
            }
            catch (...) {
                ex = std::current_exception();
            }

            return SQL_ERROR;
        };

        auto rc = CALL_WITH_TYPED_HANDLE_SKIP_DIAG(SQL_HANDLE_DESC, SourceDescHandle, func);

        if (ex)
            std::rethrow_exception(ex);

        if (rc == SQL_INVALID_HANDLE)
            throw SqlException("Invalid attribute value", "HY024");

        return rc;
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_DESC, TargetDescHandle, func);
}

SQLRETURN EndTran(
    SQLSMALLINT     handle_type,
    SQLHANDLE       handle,
    SQLSMALLINT     completion_type
) noexcept {
    auto func = [&] (auto & object) {

        // TODO: implement.

        return SQL_SUCCESS;
    };

    return CALL_WITH_TYPED_HANDLE_SKIP_DIAG(handle_type, handle, func);
}

SQLRETURN fillBinding(
    Statement & statement,
    ResultSet & result_set,
    std::size_t row_idx,
    std::size_t column_idx,
    BindingInfo binding_info
) {
    SQLINTEGER desc_type = SQL_ATTR_APP_ROW_DESC;

//  if (binding_info.c_type == SQL_APD_TYPE) {
//      desc_type = SQL_ATTR_APP_PARAM_DESC;
//      throw SqlException("Unable to read output parameter data");
//  }

    if (
        binding_info.c_type == SQL_ARD_TYPE ||
//      binding_info.c_type == SQL_APD_TYPE ||
        binding_info.c_type == SQL_C_DEFAULT
    ) {
        const auto column_num = column_idx + 1;
        auto & desc = statement.getEffectiveDescriptor(desc_type);
        auto & record = desc.getRecord(column_num, desc_type);

        binding_info.c_type = record.getAttrAs<SQLSMALLINT>(SQL_DESC_CONCISE_TYPE, SQL_C_DEFAULT);
    }

    if (binding_info.c_type == SQL_C_DEFAULT) {
        const auto & column_info = result_set.getColumnInfo(column_idx);
        binding_info.c_type = convertSQLTypeToCType(statement.getTypeInfo(column_info.type, column_info.type_without_parameters).data_type);
    }

    if (
        binding_info.c_type == SQL_C_NUMERIC &&
        binding_info.precision == 0
    ) {
        const auto column_num = column_idx + 1;
        auto & desc = statement.getEffectiveDescriptor(desc_type);
        auto & record = desc.getRecord(column_num, desc_type);

        binding_info.precision = record.getAttrAs<SQLSMALLINT>(SQL_DESC_PRECISION, 38);
        binding_info.scale = record.getAttrAs<SQLSMALLINT>(SQL_DESC_SCALE, 0);
    }

    return result_set.extractField(row_idx, column_idx, binding_info);
}

SQLRETURN fetchBindings(
    Statement & statement,
    SQLSMALLINT orientation,
    SQLLEN offset
) {
    auto & ard_desc = statement.getEffectiveDescriptor(SQL_ATTR_APP_ROW_DESC);
    auto & ird_desc = statement.getEffectiveDescriptor(SQL_ATTR_IMP_ROW_DESC);

    const auto row_set_size = ard_desc.getAttrAs<SQLULEN>(SQL_DESC_ARRAY_SIZE, 1);
    auto * rows_fetched_ptr = ird_desc.getAttrAs<SQLULEN *>(SQL_DESC_ROWS_PROCESSED_PTR, 0);
    auto * array_status_ptr = ird_desc.getAttrAs<SQLUSMALLINT *>(SQL_DESC_ARRAY_STATUS_PTR, 0);

    if (rows_fetched_ptr)
        *rows_fetched_ptr = 0;

    if (!statement.hasResultSet() || row_set_size == 0) {
        if (array_status_ptr) {
            for (std::size_t row_idx = 0; row_idx < row_set_size; ++row_idx) {
                array_status_ptr[row_idx] = SQL_ROW_NOROW;
            }
        }

        return SQL_NO_DATA;
    }

    auto & result_set = statement.getResultSet();
    const auto rows_fetched = result_set.fetchRowSet(orientation, offset, row_set_size);

    if (rows_fetched == 0) {
        statement.getDiagHeader().setAttr(SQL_DIAG_ROW_COUNT, result_set.getAffectedRowCount());

        if (array_status_ptr) {
            for (std::size_t row_idx = 0; row_idx < row_set_size; ++row_idx) {
                array_status_ptr[row_idx] = SQL_ROW_NOROW;
            }
        }

        return SQL_NO_DATA;
    }

    if (rows_fetched_ptr)
        *rows_fetched_ptr = rows_fetched;

    const auto ard_record_count = ard_desc.getRecordCount();
    ard_desc.getRecord(ard_record_count, SQL_ATTR_APP_ROW_DESC); // ...just to make sure that record container is in sync with SQL_DESC_COUNT.
    const auto & ard_records = ard_desc.getRecordContainer(); // ...only for faster access in a loop.

    const auto bind_type = ard_desc.getAttrAs<SQLULEN>(SQL_DESC_BIND_TYPE, SQL_BIND_TYPE_DEFAULT);
    const auto * bind_offset_ptr = ard_desc.getAttrAs<SQLULEN *>(SQL_DESC_BIND_OFFSET_PTR, 0);
    const auto bind_offset = (bind_offset_ptr ? *bind_offset_ptr : 0);

    bool success_with_info_met = false;
    std::size_t error_num = 0;

    BindingInfo * base_bindings = nullptr;
    if (ard_record_count > 0) {
        base_bindings = static_cast<BindingInfo *>(stack_alloc(ard_record_count * sizeof(BindingInfo)));
        if (base_bindings == nullptr)
            throw std::bad_alloc();
        std::fill(base_bindings, base_bindings + ard_record_count, BindingInfo{});
    }

    // Prepare the base binding info for all columns before iterating over the row set.
    for (std::size_t column_num = 1; column_num <= ard_record_count; ++column_num) { // Skipping the bookmark (0) column.
        const auto column_idx = column_num - 1;
        const auto & ard_record = ard_records[column_num];
        auto & base_binding = base_bindings[column_idx];

        base_binding.c_type = ard_record.getAttrAs<SQLSMALLINT>(SQL_DESC_CONCISE_TYPE, SQL_C_DEFAULT);
        base_binding.value = ard_record.getAttrAs<SQLPOINTER>(SQL_DESC_DATA_PTR, 0);
        base_binding.value_max_size = ard_record.getAttrAs<SQLLEN>(SQL_DESC_OCTET_LENGTH, 0);
        base_binding.value_size = ard_record.getAttrAs<SQLLEN *>(SQL_DESC_OCTET_LENGTH_PTR, 0);
        base_binding.indicator = ard_record.getAttrAs<SQLLEN *>(SQL_DESC_INDICATOR_PTR, 0);
    }

    for (std::size_t row_idx = 0; row_idx < rows_fetched; ++row_idx) {
        for (std::size_t column_num = 1; column_num <= ard_record_count; ++column_num) { // Skipping the bookmark (0) column.
            const auto column_idx = column_num - 1;
            auto & base_binding = base_bindings[column_idx];
            SQLRETURN code = SQL_SUCCESS;

            if (
                base_binding.value ||
                base_binding.value_size ||
                base_binding.indicator
            ) { // Only if the column is bound...
                const auto next_value_ptr_increment = (bind_type == SQL_BIND_BY_COLUMN ? base_binding.value_max_size : bind_type);
                const auto next_sz_ind_ptr_increment = (bind_type == SQL_BIND_BY_COLUMN ? sizeof(SQLLEN) : bind_type);

                BindingInfo binding_info;
                binding_info.c_type = base_binding.c_type;
                binding_info.value_max_size = base_binding.value_max_size;
                binding_info.value = (SQLPOINTER)(base_binding.value ? ((char *)(base_binding.value) + row_idx * next_value_ptr_increment + bind_offset) : 0);
                binding_info.value_size = (SQLLEN *)(base_binding.value_size ? ((char *)(base_binding.value_size) + row_idx * next_sz_ind_ptr_increment + bind_offset) : 0);
                binding_info.indicator = (SQLLEN *)(base_binding.indicator ? ((char *)(base_binding.indicator) + row_idx * next_sz_ind_ptr_increment + bind_offset) : 0);

                // TODO: fill per-row and per-column diagnostics on (some soft?) errors.
                const auto code = fillBinding(
                    statement,
                    result_set,
                    row_idx,
                    column_idx,
                    binding_info
                );
            }

            switch (code) {
                case SQL_SUCCESS: {
                    if (array_status_ptr)
                        array_status_ptr[row_idx] = SQL_ROW_SUCCESS;

                    break;
                }

                case SQL_SUCCESS_WITH_INFO: {
                    success_with_info_met = true;

                    if (array_status_ptr)
                        array_status_ptr[row_idx] = SQL_ROW_SUCCESS_WITH_INFO;

                    break;
                }

                default: {
                    ++error_num;

                    if (array_status_ptr)
                        array_status_ptr[row_idx] = SQL_ROW_ERROR;

                    break;
                }
            }
        }
    }

    if (array_status_ptr) {
        for (std::size_t row_idx = rows_fetched; row_idx < row_set_size; ++row_idx) {
            array_status_ptr[row_idx] = SQL_ROW_NOROW;
        }
    }

    if (error_num >= rows_fetched)
        return SQL_ERROR;

    if (error_num > 0 || success_with_info_met)
        return SQL_SUCCESS_WITH_INFO;

    return SQL_SUCCESS;
}

SQLRETURN GetData(
    SQLHSTMT       StatementHandle,
    SQLUSMALLINT   Col_or_Param_Num,
    SQLSMALLINT    TargetType,
    SQLPOINTER     TargetValuePtr,
    SQLLEN         BufferLength,
    SQLLEN *       StrLen_or_IndPtr
) noexcept {
    auto func = [&] (Statement & statement) {
        if (!statement.hasResultSet())
            throw SqlException("Column info is not available", "07005");

        auto & result_set = statement.getResultSet();

        if (result_set.getCurrentRowPosition() < 1)
            throw SqlException("Invalid cursor state", "24000");

        if (Col_or_Param_Num < 1)
            throw SqlException("Invalid descriptor index", "07009");

        const auto row_idx = result_set.getCurrentRowPosition() - result_set.getCurrentRowSetPosition();
        const auto column_idx = Col_or_Param_Num - 1;

        BindingInfo binding_info;
        binding_info.c_type = TargetType;
        binding_info.value = TargetValuePtr;
        binding_info.value_max_size = BufferLength;
        binding_info.value_size = StrLen_or_IndPtr;
        binding_info.indicator = StrLen_or_IndPtr;

        return fillBinding(statement, result_set, row_idx, column_idx, binding_info);
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, StatementHandle, func);
}

SQLRETURN Fetch(
    SQLHSTMT       StatementHandle
) noexcept {
    auto func = [&] (Statement & statement) {
        return fetchBindings(statement, SQL_FETCH_NEXT, 0);
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, StatementHandle, func);
}

SQLRETURN FetchScroll(
    SQLHSTMT      StatementHandle,
    SQLSMALLINT   FetchOrientation,
    SQLLEN        FetchOffset
) noexcept {
    auto func = [&] (Statement & statement) {
        return fetchBindings(statement, FetchOrientation, FetchOffset);
    };

    return CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, StatementHandle, func);
}

SQLRETURN getServerVersion(
     SQLHDBC         hdbc,
     SQLPOINTER      buffer_ptr,
     SQLSMALLINT     buffer_len,
     SQLSMALLINT *   string_length_ptr
)
{
    SQLRETURN res = SQL_SUCCESS;
    Connection * dbc = static_cast<Connection*>(hdbc);

    // Because the server version is requested from the connection handle,
    // create a temporary statement handle here, which is closed before the
    // function completes.
    Statement * stmt = nullptr;
    res = impl::allocStmt((SQLHDBC)dbc, (SQLHSTMT *)&stmt);
    if (!SQL_SUCCEEDED(res))
        return res; // stmt is null, no need to free it

    res = CALL_WITH_TYPED_HANDLE( SQL_HANDLE_STMT, stmt, [](Statement & stmt) {
        const auto query = toUTF8("select version()");
        stmt.executeQuery(query);
        return SQL_SUCCESS;
    });

    if (SQL_SUCCEEDED(res))
    {
        res = impl::Fetch(stmt);

        if (SQL_SUCCEEDED(res))
        {
            SQLLEN indicator = 0;

            //  Binds `buffer_ptr` and other parameters directly to the result of the query
            res = impl::GetData(
                stmt,
                1,
                getCTypeFor<SQLTCHAR*>(),
                buffer_ptr,
                buffer_len,
                &indicator
            );

            if (string_length_ptr)
                *string_length_ptr = indicator;

            if (indicator < 0)
            {
                // The server returned NULL or something very unexpected happened
                stmt->fillDiag(SQL_ERROR, "HY000", "Unexpected server version", 1);
                res = SQL_ERROR;
            }
        }
    }

    // All diagnostic records from the statement handle must be copied to
    // the connection handle so that clients can still access the error and
    // the warning messages.
    copyDiagnosticsRecords(*stmt, *dbc);
    dbc->setReturnCode(res);
    impl::freeHandle(stmt);
    return res;
}

} // namespace impl
