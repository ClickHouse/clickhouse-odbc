#include "driver.h"
#include "utils.h"
#include "scope_guard.h"
#include "string_ref.h"
#include "type_parser.h"
#include "attributes.h"
#include "diagnostics.h"
#include "environment.h"
#include "connection.h"
#include "descriptor.h"
#include "statement.h"
#include "result_set.h"

#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cstring>

#include <Poco/Net/HTTPClientSession.h>

/** Functions from the ODBC interface can not directly call other functions.
  * Because not a function from this library will be called, but a wrapper from the driver manager,
  * which can work incorrectly, being called from within another function.
  * Wrong - because driver manager wraps all handle in its own,
  * which already have other addresses.
  */

namespace { namespace impl {

SQLRETURN GetDiagRec(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLTCHAR * out_sqlstate,
    SQLINTEGER * out_native_error_code,
    SQLTCHAR * out_mesage,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size
) noexcept {
    auto func = [&] (auto & object) -> SQLRETURN {
        if (record_number < 1 || out_message_max_size < 0)
            return SQL_ERROR;

        if (record_number > object.get_diag_status_count())
            return SQL_NO_DATA;

        const auto & record = object.get_diag_status(record_number);

        /// The five-letter SQLSTATE and the trailing zero.
        if (out_sqlstate) {
            std::size_t size = 6;
            std::size_t written = 0;
            fillOutputPlatformString(record.template get_attr_as<std::string>(SQL_DIAG_SQLSTATE), out_sqlstate, size, &written, true);
        }

        if (out_native_error_code != nullptr) {
            *out_native_error_code = record.template get_attr_as<SQLINTEGER>(SQL_DIAG_NATIVE);
        }

        return fillOutputPlatformString(record.template get_attr_as<std::string>(SQL_DIAG_MESSAGE_TEXT), out_mesage, out_message_max_size, out_message_size, true);
    };

    return CALL_WITH_TYPED_HANDLE_SKIP_DIAG(handle_type, handle, func);
}

SQLRETURN GetDiagField(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLSMALLINT field_id,
    SQLPOINTER out_mesage,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size
) noexcept {
    auto func = [&] (auto & object) -> SQLRETURN {
        // Exit with error if the requested field is relevant only to statements.
        if (get_object_handle_type<decltype(object)>() != SQL_HANDLE_STMT) {
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

        if (record_number > 0 && record_number > object.get_diag_status_count())
            return SQL_NO_DATA;

        const auto & record = object.get_diag_status(record_number);

        switch (field_id) {

#define CASE_ATTR_NUM(NAME, TYPE) \
    case NAME: \
        return fillOutputNumber(record.template get_attr_as<TYPE>(NAME), out_mesage, out_message_max_size, out_message_size);

#define CASE_ATTR_STR(NAME) \
    case NAME: \
        return fillOutputPlatformString(record.template get_attr_as<std::string>(NAME), out_mesage, out_message_max_size, out_message_size);

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
        auto & apd_desc = statement.get_effective_descriptor(SQL_ATTR_APP_PARAM_DESC);
        auto & ipd_desc = statement.get_effective_descriptor(SQL_ATTR_IMP_PARAM_DESC);
        const auto apd_record_count = apd_desc.get_record_count();
        const auto ipd_record_count = ipd_desc.get_record_count();
        auto & apd_record = apd_desc.get_record(parameter_number, SQL_ATTR_APP_PARAM_DESC);
        auto & ipd_record = ipd_desc.get_record(parameter_number, SQL_ATTR_IMP_PARAM_DESC);

        try {
            ipd_record.set_attr(SQL_DESC_PARAMETER_TYPE, input_output_type);

            // These two will trigger automatic (re)setting of SQL_DESC_TYPE and SQL_DESC_DATETIME_INTERVAL_CODE,
            // and resetting of SQL_DESC_DATA_PTR.
            apd_record.set_attr(SQL_DESC_CONCISE_TYPE,
                (value_type == SQL_C_DEFAULT ? convert_sql_type_to_C_type(parameter_type) : value_type)
            );
            ipd_record.set_attr(SQL_DESC_CONCISE_TYPE, parameter_type);

            switch (parameter_type) {
                case SQL_CHAR:
                case SQL_VARCHAR:
                case SQL_LONGVARCHAR:
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
                    ipd_record.set_attr(SQL_DESC_LENGTH, column_size);
                    break;

                case SQL_DECIMAL:
                case SQL_NUMERIC:
                case SQL_FLOAT:
                case SQL_REAL:
                case SQL_DOUBLE:
                    ipd_record.set_attr(SQL_DESC_PRECISION, column_size);
                    break;
            }

            switch (parameter_type) {
                case SQL_TYPE_TIME:
                case SQL_TYPE_TIMESTAMP:
                case SQL_INTERVAL_SECOND:
                case SQL_INTERVAL_DAY_TO_SECOND:
                case SQL_INTERVAL_HOUR_TO_SECOND:
                case SQL_INTERVAL_MINUTE_TO_SECOND:
                    ipd_record.set_attr(SQL_DESC_PRECISION, decimal_digits);
                    break;

                case SQL_NUMERIC:
                case SQL_DECIMAL:
                    ipd_record.set_attr(SQL_DESC_SCALE, decimal_digits);
                    break;
            }

            apd_record.set_attr(SQL_DESC_DATA_PTR, parameter_value_ptr);
            apd_record.set_attr(SQL_DESC_OCTET_LENGTH, buffer_length);
            apd_record.set_attr(SQL_DESC_OCTET_LENGTH_PTR, StrLen_or_IndPtr);
            apd_record.set_attr(SQL_DESC_INDICATOR_PTR, StrLen_or_IndPtr);
        }
        catch (...) {
            apd_desc.set_attr(SQL_DESC_COUNT, apd_record_count);
            ipd_desc.set_attr(SQL_DESC_COUNT, ipd_record_count);

            throw;
        }

        return SQL_SUCCESS;
    };

    return CALL_WITH_HANDLE(handle, func);
}

SQLRETURN NumParams(
    SQLHSTMT        handle,
    SQLSMALLINT *   out_parameter_count
) noexcept {
    auto func = [&] (Statement & statement) {
        *out_parameter_count = statement.get_effective_descriptor(SQL_ATTR_IMP_PARAM_DESC).get_attr_as<SQLSMALLINT>(SQL_DESC_COUNT);
        return SQL_SUCCESS;
    };

    return CALL_WITH_HANDLE(handle, func);
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
        auto & ipd_desc = statement.get_effective_descriptor(SQL_ATTR_IMP_PARAM_DESC);

        if (parameter_number < 0 || parameter_number > ipd_desc.get_record_count())
            throw SqlException("Invalid descriptor index", "07009");

        auto & ipd_record = ipd_desc.get_record(parameter_number, SQL_ATTR_IMP_PARAM_DESC);

        *out_data_type_ptr = ipd_record.get_attr_as<SQLSMALLINT>(SQL_DESC_CONCISE_TYPE);
        *out_nullable_ptr = ipd_record.get_attr_as<SQLSMALLINT>(SQL_DESC_NULLABLE);

        if (ipd_record.has_column_size())
            *out_parameter_size_ptr = ipd_record.get_column_size();

        if (ipd_record.has_decimal_digits())
            *out_decimal_digits_ptr = ipd_record.get_decimal_digits();

        return SQL_SUCCESS;
    };

    return CALL_WITH_HANDLE(handle, func);
}

} } // namespace impl


extern "C" {

/// Description: https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlconnect-function
RETCODE SQL_API FUNCTION_MAYBE_W(SQLConnect)(HDBC connection_handle,
    SQLTCHAR * dsn,
    SQLSMALLINT dsn_size,
    SQLTCHAR * user,
    SQLSMALLINT user_size,
    SQLTCHAR * password,
    SQLSMALLINT password_size) {
    // LOG(__FUNCTION__ << " dsn_size=" << dsn_size << " dsn=" << dsn << " user_size=" << user_size << " user=" << user << " password_size=" << password_size << " password=" << password);

    return CALL_WITH_HANDLE(connection_handle, [&](Connection & connection) {
        std::string dsn_str = stringFromSQLSymbols(dsn, dsn_size);
        std::string user_str = stringFromSQLSymbols(user, user_size);
        std::string password_str = stringFromSQLSymbols(password, password_size);

        LOG(__FUNCTION__ << " dsn=" << dsn_str << " user=" << user_str << " pwd=" << password_str);

        connection.init(dsn_str, 0, user_str, password_str, "");
        return SQL_SUCCESS;
    });
}


/// Description: https://docs.microsoft.com/en-us/sql/relational-databases/native-client-odbc-api/sqldriverconnect
RETCODE SQL_API FUNCTION_MAYBE_W(SQLDriverConnect)(HDBC connection_handle,
    HWND unused_window,
    SQLTCHAR FAR * connection_str_in,
    SQLSMALLINT connection_str_in_size,
    SQLTCHAR FAR * connection_str_out,
    SQLSMALLINT connection_str_out_max_size,
    SQLSMALLINT FAR * connection_str_out_size,
    SQLUSMALLINT driver_completion) {
    LOG(__FUNCTION__ << " connection_str_in=" << connection_str_in << " : " << connection_str_in_size
                     << /* " connection_str_out=" << connection_str_out << */ " " << connection_str_out_max_size);

    return CALL_WITH_HANDLE(connection_handle, [&](Connection & connection) {
        // if (connection_str_in_size > 0) hex_print(log_stream, std::string{static_cast<const char *>(static_cast<const void *>(connection_str_in)), static_cast<size_t>(connection_str_in_size)});
        auto connection_str = stringFromSQLSymbols2(connection_str_in, connection_str_in_size);
        // LOG("connection_str=" << str);
        connection.init(connection_str);
        // Copy complete connection string.
        fillOutputPlatformString(
            connection.connectionString(), connection_str_out, connection_str_out_max_size, connection_str_out_size, false);
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLPrepare)(HSTMT statement_handle, SQLTCHAR * statement_text, SQLINTEGER statement_text_size) {
    LOG(__FUNCTION__ << " statement_text_size=" << statement_text_size << " statement_text=" << statement_text);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        const auto query = stringFromSQLSymbols2(statement_text, statement_text_size);
        statement.prepareQuery(query);
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLExecute(HSTMT statement_handle) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        statement.executeQuery();
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLExecDirect)(HSTMT statement_handle, SQLTCHAR * statement_text, SQLINTEGER statement_text_size) {
    LOG(__FUNCTION__ << " statement_text_size=" << statement_text_size << " statement_text=" << statement_text);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        const auto query = stringFromSQLSymbols(statement_text, statement_text_size);
        statement.executeQuery(query);
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLNumResultCols(HSTMT statement_handle, SQLSMALLINT * column_count) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        if (column_count) {
            if (statement.has_result_set()) {
                *column_count = statement.getNumColumns();
            }
            else {
                *column_count = 0;
            }
        }
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLColAttribute(HSTMT statement_handle,
    SQLUSMALLINT column_number,
    SQLUSMALLINT field_identifier,
    SQLPOINTER out_string_value,
    SQLSMALLINT out_string_value_max_size,
    SQLSMALLINT * out_string_value_size,
#if defined(_unix_) || defined(_win64_) || defined(__FreeBSD__)
    SQLLEN *
#else
    SQLPOINTER
#endif
        out_num_value) {
    LOG(__FUNCTION__ << "(col=" << column_number << ", field=" << field_identifier << ")");

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) -> RETCODE {
        if (!statement.has_result_set())
            throw SqlException("Column info is not available", "07009");

        if (column_number < 1 || column_number > statement.getNumColumns())
            throw SqlException("Column number " + std::to_string(column_number) + " is out of range: 1.." +
                std::to_string(statement.getNumColumns()), "07009");

        const auto column_idx = column_number - 1;

        SQLLEN num_value = 0;
        std::string str_value;

        const ColumnInfo & column_info = statement.getColumnInfo(column_idx);
        const TypeInfo & type_info = statement.getTypeInfo(column_info.type, column_info.type_without_parameters);

        switch (field_identifier) {
            case SQL_DESC_AUTO_UNIQUE_VALUE:
                num_value = SQL_FALSE;
                break;
            case SQL_DESC_BASE_COLUMN_NAME:
                str_value = column_info.name;
                break;
            case SQL_DESC_BASE_TABLE_NAME:
                break;
            case SQL_DESC_CASE_SENSITIVE:
                num_value = SQL_TRUE;
                break;
            case SQL_DESC_CATALOG_NAME:
                break;
            case SQL_DESC_CONCISE_TYPE:
                num_value = type_info.sql_type;
                break;
            case SQL_DESC_COUNT:
                num_value = statement.getNumColumns();
                break;
            case SQL_DESC_DISPLAY_SIZE:
                // TODO (artpaul) https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/display-size
                num_value = column_info.display_size;
                break;
            case SQL_DESC_FIXED_PREC_SCALE:
                num_value = SQL_FALSE;
                break;
            case SQL_DESC_LABEL:
                str_value = column_info.name;
                break;
            case SQL_DESC_LENGTH:
                if (type_info.IsStringType())
                    num_value = std::min<int32_t>(
                        statement.get_parent().stringmaxlength, column_info.fixed_size ? column_info.fixed_size : column_info.display_size);
                break;
            case SQL_DESC_LITERAL_PREFIX:
                break;
            case SQL_DESC_LITERAL_SUFFIX:
                break;
            case SQL_DESC_LOCAL_TYPE_NAME:
                break;
            case SQL_DESC_NAME:
                str_value = column_info.name;
                break;
            case SQL_DESC_NULLABLE:
                num_value = column_info.is_nullable;
                break;
            case SQL_DESC_OCTET_LENGTH:
                if (type_info.IsStringType())
                    num_value = std::min<int32_t>(statement.get_parent().stringmaxlength,
                                    column_info.fixed_size ? column_info.fixed_size : column_info.display_size)
                        * SIZEOF_CHAR;
                else
                    num_value = type_info.octet_length;
                break;
            case SQL_DESC_PRECISION:
                num_value = 0;
                break;
            case SQL_DESC_NUM_PREC_RADIX:
                if (type_info.IsIntegerType())
                    num_value = 10;
                break;
            case SQL_DESC_SCALE:
                break;
            case SQL_DESC_SCHEMA_NAME:
                break;
            case SQL_DESC_SEARCHABLE:
                num_value = SQL_SEARCHABLE;
                break;
            case SQL_DESC_TABLE_NAME:
                break;
            case SQL_DESC_TYPE:
                num_value = type_info.sql_type;
                break;
            case SQL_DESC_TYPE_NAME:
                str_value = type_info.sql_type_name;
                break;
            case SQL_DESC_UNNAMED:
                num_value = SQL_NAMED;
                break;
            case SQL_DESC_UNSIGNED:
                num_value = type_info.is_unsigned;
                break;
            case SQL_DESC_UPDATABLE:
                num_value = SQL_FALSE;
                break;
            default:
                LOG(__FUNCTION__ << ": Unsupported FieldIdentifier = " + std::to_string(field_identifier));
                throw SqlException("Unsupported FieldIdentifier = " + std::to_string(field_identifier), "HYC00");
        }

        if (out_num_value)
            memcpy(out_num_value, &num_value, sizeof(SQLLEN));

        LOG(__FUNCTION__ << " num_value=" << num_value << " str_value=" << str_value);

        return fillOutputPlatformString(str_value, out_string_value, out_string_value_max_size, out_string_value_size);
    });
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLDescribeCol)(HSTMT statement_handle,
    SQLUSMALLINT column_number,
    SQLTCHAR * out_column_name,
    SQLSMALLINT out_column_name_max_size,
    SQLSMALLINT * out_column_name_size,
    SQLSMALLINT * out_type,
    SQLULEN * out_column_size,
    SQLSMALLINT * out_decimal_digits,
    SQLSMALLINT * out_is_nullable) {
    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        if (!statement.has_result_set())
            throw SqlException("Column info is not available", "07009");

        if (column_number < 1 || column_number > statement.getNumColumns())
            throw SqlException("Column number " + std::to_string(column_number) + " is out of range: 1.." +
                std::to_string(statement.getNumColumns()), "07009");

        const auto column_idx = column_number - 1;

        const ColumnInfo & column_info = statement.getColumnInfo(column_idx);
        const TypeInfo & type_info = statement.getTypeInfo(column_info.type, column_info.type_without_parameters);

        LOG(__FUNCTION__ << " column_number=" << column_number << "name=" << column_info.name << " type=" << type_info.sql_type
                         << " size=" << type_info.column_size << " nullable=" << column_info.is_nullable);

        if (out_type)
            *out_type = type_info.sql_type;
        if (out_column_size)
            *out_column_size = std::min<int32_t>(
                statement.get_parent().stringmaxlength, column_info.fixed_size ? column_info.fixed_size : type_info.column_size);
        if (out_decimal_digits)
            *out_decimal_digits = 0;
        if (out_is_nullable)
            *out_is_nullable = column_info.is_nullable ? SQL_NULLABLE : SQL_NO_NULLS;

        return fillOutputPlatformString(column_info.name, out_column_name, out_column_name_max_size, out_column_name_size, false);
    });
}


RETCODE SQL_API impl_SQLGetData(HSTMT statement_handle,
    SQLUSMALLINT column_or_param_number,
    SQLSMALLINT target_type,
    PTR out_value,
    SQLLEN out_value_max_size,
    SQLLEN * out_value_size_or_indicator) {
    LOG(__FUNCTION__ << " column_or_param_number=" << column_or_param_number << " target_type=" << target_type);
#ifndef NDEBUG
    SCOPE_EXIT({ LOG("impl_SQLGetData finish."); }); // for timing only
#endif

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) -> RETCODE {
        if (!statement.has_result_set())
            throw SqlException("Column info is not available", "07009");

        if (column_or_param_number < 1 || column_or_param_number > statement.getNumColumns())
            throw SqlException("Column number " + std::to_string(column_or_param_number) + " is out of range: 1.." +
                std::to_string(statement.getNumColumns()), "07009");

        if (!statement.has_current_row())
            throw SqlException("Invalid cursor state", "24000");

        const auto column_idx = column_or_param_number - 1;

        const Field & field = statement.get_current_row().data[column_idx];

        LOG("column: " << column_idx << ", target_type: " << target_type << ", out_value_max_size: " << out_value_max_size
                       << " null=" << field.is_null << " data=" << field.data);

        if (field.is_null)
            return fillOutputNULL(out_value, out_value_max_size, out_value_size_or_indicator);

        switch (target_type) {
            case SQL_C_CHAR:
            case SQL_C_BINARY:
                return fillOutputRawString(field.data, out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_WCHAR:
                return fillOutputUSC2String(field.data, out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_TINYINT:
            case SQL_C_STINYINT:
                return fillOutputNumber<int8_t>(field.getInt(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_UTINYINT:
            case SQL_C_BIT:
                return fillOutputNumber<uint8_t>(field.getUInt(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_SHORT:
            case SQL_C_SSHORT:
                return fillOutputNumber<int16_t>(field.getInt(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_USHORT:
                return fillOutputNumber<uint16_t>(field.getUInt(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_LONG:
            case SQL_C_SLONG:
                return fillOutputNumber<int32_t>(field.getInt(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_ULONG:
                return fillOutputNumber<uint32_t>(field.getUInt(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_SBIGINT:
                return fillOutputNumber<int64_t>(field.getInt(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_UBIGINT:
                return fillOutputNumber<uint64_t>(field.getUInt(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_FLOAT:
                return fillOutputNumber<float>(field.getFloat(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_DOUBLE:
                return fillOutputNumber<double>(field.getDouble(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_DATE:
            case SQL_C_TYPE_DATE:
                return fillOutputNumber<SQL_DATE_STRUCT>(field.getDate(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_TIMESTAMP:
            case SQL_C_TYPE_TIMESTAMP:
                return fillOutputNumber<SQL_TIMESTAMP_STRUCT>(
                    field.getDateTime(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_ARD_TYPE:
            case SQL_C_DEFAULT:
                LOG(__FUNCTION__ << ": Unsupported type requested (throw)." << target_type);
                throw std::runtime_error("Unsupported type requested.");

            default:
                LOG(__FUNCTION__ << ": Unknown type requested (throw)." << target_type);
                throw std::runtime_error("Unknown type requested.");
        }
    });
}


RETCODE
impl_SQLFetch(HSTMT statement_handle) {
    LOG(__FUNCTION__);
#ifndef NDEBUG
    SCOPE_EXIT({ LOG("impl_SQLFetch finish."); }); // for timing only
#endif

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) -> RETCODE {
        auto * rows_fetched_ptr = statement.get_effective_descriptor(SQL_ATTR_IMP_ROW_DESC).get_attr_as<SQLULEN *>(SQL_DESC_ROWS_PROCESSED_PTR, 0);

        if (rows_fetched_ptr)
            *rows_fetched_ptr = 0;

        if (!statement.has_result_set())
            return SQL_NO_DATA;

        if (!statement.advance_to_next_row())
            return SQL_NO_DATA;

        if (rows_fetched_ptr)
            *rows_fetched_ptr = 1;

        // LOG("impl_SQLFetch statement.bindings.size()=" << statement.bindings.size());

        auto res = SQL_SUCCESS;

        for (auto & col_num_binding : statement.bindings) {
            auto code = impl_SQLGetData(statement_handle,
                col_num_binding.first,
                col_num_binding.second.type,
                col_num_binding.second.value,
                col_num_binding.second.value_max_size,
                col_num_binding.second.value_size/* or .indicator */);

            if (code == SQL_SUCCESS_WITH_INFO)
                res = code;
            else if (code != SQL_SUCCESS)
                return code;
        }

        return res;
    });
}


RETCODE SQL_API SQLFetch(HSTMT statement_handle) {
    return impl_SQLFetch(statement_handle);
}


RETCODE SQL_API SQLFetchScroll(HSTMT statement_handle, SQLSMALLINT orientation, SQLLEN offset) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) -> RETCODE {
        if (orientation != SQL_FETCH_NEXT)
            throw SqlException("Fetch type out of range", "HY106");

        return impl_SQLFetch(statement_handle);
    });
}


RETCODE SQL_API SQLGetData(HSTMT statement_handle,
    SQLUSMALLINT column_or_param_number,
    SQLSMALLINT target_type,
    PTR out_value,
    SQLLEN out_value_max_size,
    SQLLEN * out_value_size_or_indicator) {
    return impl_SQLGetData(
        statement_handle, column_or_param_number, target_type, out_value, out_value_max_size, out_value_size_or_indicator);
}


RETCODE SQL_API SQLBindCol(HSTMT statement_handle,
    SQLUSMALLINT column_number,
    SQLSMALLINT target_type,
    PTR out_value,
    SQLLEN out_value_max_size,
    SQLLEN * out_value_size_or_indicator) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        if (out_value_max_size < 0)
            throw SqlException("Invalid string or buffer length", "HY090");

        if (!statement.has_result_set())
            throw SqlException("Column info is not available", "07009");

        if (column_number < 1 || column_number > statement.getNumColumns())
            throw SqlException("Column number " + std::to_string(column_number) + " is out of range: 1.." +
                std::to_string(statement.getNumColumns()), "07009");

        // Unbinding column
        if (out_value_size_or_indicator == nullptr) {
            statement.bindings.erase(column_number);
            return SQL_SUCCESS;
        }

        const auto column_idx = column_number - 1;

        if (target_type == SQL_C_DEFAULT) {
            target_type = statement.getTypeInfo(statement.getColumnInfo(column_idx).type_without_parameters).sql_type;
        }

        BindingInfo binding;
        binding.type = target_type;
        binding.value = out_value;
        binding.value_max_size = out_value_max_size;
        binding.value_size = out_value_size_or_indicator;
        binding.indicator = out_value_size_or_indicator;

        statement.bindings[column_number] = binding;

        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLRowCount(HSTMT statement_handle, SQLLEN * out_row_count) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        if (out_row_count) {
            *out_row_count = statement.get_diag_header().get_attr_as<SQLLEN>(SQL_DIAG_ROW_COUNT, 0);
            LOG("getNumRows=" << *out_row_count);
        }
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLMoreResults(HSTMT statement_handle) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        return (statement.advance_to_next_result_set() ? SQL_SUCCESS : SQL_NO_DATA);
    });
}


RETCODE SQL_API SQLDisconnect(HDBC connection_handle) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(connection_handle, [&](Connection & connection) {
        connection.session->reset();
        return SQL_SUCCESS;
    });
}


/// Description: https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdiagrec-function
SQLRETURN SQL_API FUNCTION_MAYBE_W(SQLGetDiagRec)(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLTCHAR * out_sqlstate,
    SQLINTEGER * out_native_error_code,
    SQLTCHAR * out_mesage,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size
) {
    return impl::GetDiagRec(
        handle_type,
        handle,
        record_number,
        out_sqlstate,
        out_native_error_code,
        out_mesage,
        out_message_max_size,
        out_message_size
    );
}


/// Description: https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdiagfield-function
SQLRETURN SQL_API SQLGetDiagField(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLSMALLINT field_id,
    SQLPOINTER out_mesage,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size
) {
    return impl::GetDiagField(
        handle_type,
        handle,
        record_number,
        field_id,
        out_mesage,
        out_message_max_size,
        out_message_size
    );
}


/// Description: https://docs.microsoft.com/en-us/sql/relational-databases/native-client-odbc-api/sqltables
RETCODE SQL_API FUNCTION_MAYBE_W(SQLTables)(HSTMT statement_handle,
    SQLTCHAR * catalog_name,
    SQLSMALLINT catalog_name_length,
    SQLTCHAR * schema_name,
    SQLSMALLINT schema_name_length,
    SQLTCHAR * table_name,
    SQLSMALLINT table_name_length,
    SQLTCHAR * table_type,
    SQLSMALLINT table_type_length) {
    LOG(__FUNCTION__);

    // TODO (artpaul) Take statement.getMetatadaId() into account.
    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        const std::string catalog = stringFromSQLSymbols(catalog_name, catalog_name_length);

        std::stringstream query;

        // Get a list of all tables in all databases.
        if (catalog_name != nullptr && catalog == SQL_ALL_CATALOGS && !schema_name && !table_name && !table_type) {
            query << "SELECT"
                     " database AS TABLE_CAT"
                     ", '' AS TABLE_SCHEM"
                     ", name AS TABLE_NAME"
                     ", 'TABLE' AS TABLE_TYPE"
                     ", '' AS REMARKS"
                     " FROM system.tables"
                     " ORDER BY TABLE_TYPE, TABLE_CAT, TABLE_SCHEM, TABLE_NAME";
        }
        // Get a list of all tables in the current database.
        else if (!catalog_name && !schema_name && !table_name && !table_type) {
            query << "SELECT"
                     " database AS TABLE_CAT"
                     ", '' AS TABLE_SCHEM"
                     ", name AS TABLE_NAME"
                     ", 'TABLE' AS TABLE_TYPE"
                     ", '' AS REMARKS"
                     " FROM system.tables"
                     " WHERE (database == '";
            query << statement.get_parent().getDatabase() << "')";
            query << " ORDER BY TABLE_TYPE, TABLE_CAT, TABLE_SCHEM, TABLE_NAME";
        }
        // Get a list of databases on the current connection's server.
        else if (!catalog.empty() && schema_name != nullptr && schema_name_length == 0 && table_name != nullptr && table_name_length == 0) {
            query << "SELECT"
                     " name AS TABLE_CAT"
                     ", '' AS TABLE_SCHEM"
                     ", '' AS TABLE_NAME"
                     ", '' AS TABLE_TYPE"
                     ", '' AS REMARKS"
                     " FROM system.databases"
                     " WHERE (1 == 1)";
            query << " AND TABLE_CAT LIKE '" << catalog << "'";
            query << " ORDER BY TABLE_CAT";
        } else {
            query << "SELECT"
                     " database AS TABLE_CAT"
                     ", '' AS TABLE_SCHEM"
                     ", name AS TABLE_NAME"
                     ", 'TABLE' AS TABLE_TYPE"
                     ", '' AS REMARKS"
                     " FROM system.tables"
                     " WHERE (1 == 1)";

            if (catalog_name && catalog_name_length)
                query << " AND TABLE_CAT LIKE '" << stringFromSQLSymbols(catalog_name, catalog_name_length) << "'";
            //if (schema_name_length)
            //    query << " AND TABLE_SCHEM LIKE '" << stringFromSQLSymbols(schema_name, schema_name_length) << "'";
            if (table_name && table_name_length)
                query << " AND TABLE_NAME LIKE '" << stringFromSQLSymbols(table_name, table_name_length) << "'";
            //if (table_type_length)
            //    query << " AND TABLE_TYPE = '" << stringFromSQLSymbols(table_type, table_type_length) << "'";

            query << " ORDER BY TABLE_TYPE, TABLE_CAT, TABLE_SCHEM, TABLE_NAME";
        }

        statement.executeQuery(query.str());
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLColumns)(HSTMT statement_handle,
    SQLTCHAR * catalog_name,
    SQLSMALLINT catalog_name_length,
    SQLTCHAR * schema_name,
    SQLSMALLINT schema_name_length,
    SQLTCHAR * table_name,
    SQLSMALLINT table_name_length,
    SQLTCHAR * column_name,
    SQLSMALLINT column_name_length) {
    LOG(__FUNCTION__);

    class ColumnsMutator : public IResultMutator {
    public:
        ColumnsMutator(Environment * env_) : env(env_) {}

        void UpdateColumnInfo(std::vector<ColumnInfo> * columns_info) override {
            columns_info->at(4).name = "Int16";
            columns_info->at(4).type_without_parameters = "Int16";
        }

        void UpdateRow(const std::vector<ColumnInfo> & columns_info, Row * row) override {
            ColumnInfo type_column;

            {
                TypeAst ast;
                if (TypeParser(row->data.at(4).data).parse(&ast)) {
                    assignTypeInfo(ast, &type_column);
                } else {
                    // Interprete all unknown types as String.
                    type_column.type_without_parameters = "String";
                }
            }

            const TypeInfo & type_info = env->getTypeInfo(type_column.type, type_column.type_without_parameters);

            row->data.at(4).data = std::to_string(type_info.sql_type);
            row->data.at(5).data = type_info.sql_type_name;
            row->data.at(6).data = std::to_string(type_info.column_size);
            row->data.at(13).data = std::to_string(type_info.sql_type);
            row->data.at(15).data = std::to_string(type_info.octet_length);
        }

    private:
        Environment * const env;
    };

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        std::stringstream query;

        query << "SELECT"
                 " database AS TABLE_CAT"   // 0
                 ", '' AS TABLE_SCHEM"      // 1
                 ", table AS TABLE_NAME"    // 2
                 ", name AS COLUMN_NAME"    // 3
                 ", type AS DATA_TYPE"      // 4
                 ", '' AS TYPE_NAME"        // 5
                 ", 0 AS COLUMN_SIZE"       // 6
                 ", 0 AS BUFFER_LENGTH"     // 7
                 ", 0 AS DECIMAL_DIGITS"    // 8
                 ", 0 AS NUM_PREC_RADIX"    // 9
                 ", 0 AS NULLABLE"          // 10
                 ", 0 AS REMARKS"           // 11
                 ", 0 AS COLUMN_DEF"        // 12
                 ", 0 AS SQL_DATA_TYPE"     // 13
                 ", 0 AS SQL_DATETIME_SUB"  // 14
                 ", 0 AS CHAR_OCTET_LENGTH" // 15
                 ", 0 AS ORDINAL_POSITION"  // 16
                 ", 0 AS IS_NULLABLE"       // 17
                 " FROM system.columns"
                 " WHERE (1 == 1)";

        std::string s;
        s = stringFromSQLSymbols(catalog_name, catalog_name_length);
        if (s.length() > 0) {
            query << " AND TABLE_CAT LIKE '" << s << "'";
        } else {
            query << " AND TABLE_CAT = currentDatabase()";
        }

        s = stringFromSQLSymbols(schema_name, schema_name_length);
        if (s.length() > 0)
            query << " AND TABLE_SCHEM LIKE '" << s << "'";

        s = stringFromSQLSymbols(table_name, table_name_length);
        if (s.length() > 0)
            query << " AND TABLE_NAME LIKE '" << s << "'";

        s = stringFromSQLSymbols(column_name, column_name_length);
        if (s.length() > 0)
            query << " AND COLUMN_NAME LIKE '" << s << "'";

        query << " ORDER BY TABLE_CAT, TABLE_SCHEM, TABLE_NAME, ORDINAL_POSITION";

        statement.executeQuery(query.str(), IResultMutatorPtr(new ColumnsMutator(&statement.get_parent().get_parent())));
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLGetTypeInfo(HSTMT statement_handle, SQLSMALLINT type) {
    LOG(__FUNCTION__ << "(type = " << type << ")");

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        std::stringstream query;
        query << "SELECT * FROM (";

        bool first = true;

        auto add_query_for_type = [&](const std::string & name, const TypeInfo & info) mutable {
            if (type != SQL_ALL_TYPES && type != info.sql_type)
                return;

            if (!first)
                query << " UNION ALL ";
            first = false;

            query << "SELECT"
                     " '"
                  << info.sql_type_name
                  << "' AS TYPE_NAME"
                     ", toInt16("
                  << info.sql_type
                  << ") AS DATA_TYPE"
                     ", toInt32("
                  << info.column_size
                  << ") AS COLUMN_SIZE"
                     ", '' AS LITERAL_PREFIX"
                     ", '' AS LITERAL_SUFFIX"
                     ", '' AS CREATE_PARAMS" /// TODO
                     ", toInt16("
                  << SQL_NO_NULLS
                  << ") AS NULLABLE"
                     ", toInt16("
                  << SQL_TRUE
                  << ") AS CASE_SENSITIVE"
                     ", toInt16("
                  << SQL_SEARCHABLE
                  << ") AS SEARCHABLE"
                     ", toInt16("
                  << info.is_unsigned
                  << ") AS UNSIGNED_ATTRIBUTE"
                     ", toInt16("
                  << SQL_FALSE
                  << ") AS FIXED_PREC_SCALE"
                     ", toInt16("
                  << SQL_FALSE
                  << ") AS AUTO_UNIQUE_VALUE"
                     ", TYPE_NAME AS LOCAL_TYPE_NAME"
                     ", toInt16(0) AS MINIMUM_SCALE"
                     ", toInt16(0) AS MAXIMUM_SCALE"
                     ", DATA_TYPE AS SQL_DATA_TYPE"
                     ", toInt16(0) AS SQL_DATETIME_SUB"
                     ", toInt32(10) AS NUM_PREC_RADIX" /// TODO
                     ", toInt16(0) AS INTERVAL_PRECISION";
        };

        for (const auto & name_info : statement.get_parent().get_parent().types_info) {
            add_query_for_type(name_info.first, name_info.second);
        }

        // TODO (artpaul) check current version of ODBC.
        //
        //      In ODBC 3.x, the SQL date, time, and timestamp data types
        //      are SQL_TYPE_DATE, SQL_TYPE_TIME, and SQL_TYPE_TIMESTAMP, respectively;
        //      in ODBC 2.x, the data types are SQL_DATE, SQL_TIME, and SQL_TIMESTAMP.
        {
            auto info = statement.get_parent().get_parent().getTypeInfo("Date");
            info.sql_type = SQL_DATE;
            add_query_for_type("Date", info);
        }

        {
            auto info = statement.get_parent().get_parent().getTypeInfo("DateTime");
            info.sql_type = SQL_TIMESTAMP;
            add_query_for_type("DateTime", info);
        }

        query << ") ORDER BY DATA_TYPE";

        if (first)
            query.str("SELECT 1 WHERE 0");

        statement.executeQuery(query.str());
        return SQL_SUCCESS;
    });
}


SQLRETURN SQL_API SQLNumParams(
    SQLHSTMT        StatementHandle,
    SQLSMALLINT *   ParameterCountPtr
) {
    LOG(__FUNCTION__);
    return impl::NumParams(
        StatementHandle,
        ParameterCountPtr
    );
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLNativeSql)(HDBC connection_handle,
    SQLTCHAR * query,
    SQLINTEGER query_length,
    SQLTCHAR * out_query,
    SQLINTEGER out_query_max_length,
    SQLINTEGER * out_query_length) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(connection_handle, [&](Connection & connection) {
        std::string query_str = stringFromSQLSymbols(query, query_length);
        return fillOutputPlatformString(query_str, out_query, out_query_max_length, out_query_length, false);
    });
}


RETCODE SQL_API SQLCloseCursor(HSTMT statement_handle) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) -> RETCODE {
        statement.close_cursor();
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLBrowseConnect)(HDBC connection_handle,
    SQLTCHAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLTCHAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT * pcbConnStrOut) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLCancel(HSTMT StatementHandle) {
    LOG(__FUNCTION__ << "Ignoring SQLCancel " << StatementHandle);
    return SQL_SUCCESS;
    //return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLDataSources)(HENV EnvironmentHandle,
    SQLUSMALLINT Direction,
    SQLTCHAR * ServerName,
    SQLSMALLINT BufferLength1,
    SQLSMALLINT * NameLength1,
    SQLTCHAR * Description,
    SQLSMALLINT BufferLength2,
    SQLSMALLINT * NameLength2) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLGetCursorName)(
    HSTMT StatementHandle, SQLTCHAR * CursorName, SQLSMALLINT BufferLength, SQLSMALLINT * NameLength) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


/// This function can be implemented in the driver manager.
RETCODE SQL_API SQLGetFunctions(HDBC connection_handle, SQLUSMALLINT FunctionId, SQLUSMALLINT * Supported) {
    LOG(__FUNCTION__ << ":" << __LINE__ << " " << " id=" << FunctionId << " ptr=" << Supported);

#define SET_EXISTS(x) Supported[(x) >> 4] |= (1 << ((x)&0xF))
// #define CLR_EXISTS(x) Supported[(x) >> 4] &= ~(1 << ((x) & 0xF))

    return CALL_WITH_HANDLE(connection_handle, [&](Connection & connection) -> RETCODE {
        if (FunctionId == SQL_API_ODBC3_ALL_FUNCTIONS) {
            memset(Supported, 0, sizeof(Supported[0]) * SQL_API_ODBC3_ALL_FUNCTIONS_SIZE);

            // info.cpp:
            SET_EXISTS(SQL_API_SQLGETINFO);

            // handles.cpp:
            SET_EXISTS(SQL_API_SQLALLOCHANDLE);
            SET_EXISTS(SQL_API_SQLALLOCENV);
            SET_EXISTS(SQL_API_SQLALLOCCONNECT);
            SET_EXISTS(SQL_API_SQLALLOCSTMT);
            SET_EXISTS(SQL_API_SQLFREEHANDLE);
            SET_EXISTS(SQL_API_SQLFREEENV);
            SET_EXISTS(SQL_API_SQLFREECONNECT);
            SET_EXISTS(SQL_API_SQLFREESTMT);

            // attr.cpp
            SET_EXISTS(SQL_API_SQLSETENVATTR);
            SET_EXISTS(SQL_API_SQLGETENVATTR);
            SET_EXISTS(SQL_API_SQLSETCONNECTATTR);
            SET_EXISTS(SQL_API_SQLGETCONNECTATTR);
            SET_EXISTS(SQL_API_SQLSETSTMTATTR);
            SET_EXISTS(SQL_API_SQLGETSTMTATTR);

            // odbc.cpp:
            SET_EXISTS(SQL_API_SQLCONNECT);
            SET_EXISTS(SQL_API_SQLDRIVERCONNECT);
            SET_EXISTS(SQL_API_SQLPREPARE);
            SET_EXISTS(SQL_API_SQLEXECUTE);
            SET_EXISTS(SQL_API_SQLEXECDIRECT);
            SET_EXISTS(SQL_API_SQLNUMRESULTCOLS);
            SET_EXISTS(SQL_API_SQLCOLATTRIBUTE);
            SET_EXISTS(SQL_API_SQLDESCRIBECOL);
            SET_EXISTS(SQL_API_SQLFETCH);
            SET_EXISTS(SQL_API_SQLFETCHSCROLL);
            SET_EXISTS(SQL_API_SQLGETDATA);
            SET_EXISTS(SQL_API_SQLBINDCOL);
            SET_EXISTS(SQL_API_SQLROWCOUNT);
            SET_EXISTS(SQL_API_SQLMORERESULTS);
            SET_EXISTS(SQL_API_SQLDISCONNECT);
            SET_EXISTS(SQL_API_SQLGETDIAGREC);
            SET_EXISTS(SQL_API_SQLGETDIAGFIELD);
            SET_EXISTS(SQL_API_SQLTABLES);
            SET_EXISTS(SQL_API_SQLCOLUMNS);
            SET_EXISTS(SQL_API_SQLGETTYPEINFO);
            SET_EXISTS(SQL_API_SQLNUMPARAMS);
            SET_EXISTS(SQL_API_SQLNATIVESQL);
            SET_EXISTS(SQL_API_SQLCLOSECURSOR);
            // CLR_EXISTS(SQL_API_SQLBROWSECONNECT);
            SET_EXISTS(SQL_API_SQLCANCEL);
            // SET_EXISTS(SQL_API_SQLCANCELHANDLE);
            // CLR_EXISTS(SQL_API_SQLDATASOURCES);
            // CLR_EXISTS(SQL_API_SQLGETCURSORNAME);
            SET_EXISTS(SQL_API_SQLGETFUNCTIONS);
            // CLR_EXISTS(SQL_API_SQLPARAMDATA);
            // CLR_EXISTS(SQL_API_SQLPUTDATA);
            // CLR_EXISTS(SQL_API_SQLSETCURSORNAME);
            SET_EXISTS(SQL_API_SQLBINDPARAMETER);
            SET_EXISTS(SQL_API_SQLDESCRIBEPARAM);
            // CLR_EXISTS(SQL_API_SQLSETPARAM);
            // CLR_EXISTS(SQL_API_SQLSPECIALCOLUMNS);
            // CLR_EXISTS(SQL_API_SQLSTATISTICS);
            // CLR_EXISTS(SQL_API_SQLCOLUMNPRIVILEGES);

            /// TODO: more here, but all not implemented

            return SQL_SUCCESS;
        } else if (FunctionId == SQL_API_ALL_FUNCTIONS) {
            //memset(Supported, 0, sizeof(Supported[0]) * 100);
            return SQL_ERROR;
        } else {
/*
		switch (FunctionId) {
			case SQL_API_SQLBINDCOL:
				*Supported = SQL_TRUE;
				break;
			default:
				*Supported = SQL_FALSE;
				break;
		}
*/
            return SQL_ERROR;
        }

        return SQL_ERROR;
    });

#undef SET_EXISTS
// #undef CLR_EXISTS
}


RETCODE SQL_API SQLParamData(HSTMT StatementHandle, PTR * Value) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLPutData(HSTMT StatementHandle, PTR Data, SQLLEN StrLen_or_Ind) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLSetCursorName)(HSTMT StatementHandle, SQLTCHAR * CursorName, SQLSMALLINT NameLength) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLSetParam(HSTMT StatementHandle,
    SQLUSMALLINT ParameterNumber,
    SQLSMALLINT ValueType,
    SQLSMALLINT ParameterType,
    SQLULEN LengthPrecision,
    SQLSMALLINT ParameterScale,
    PTR ParameterValue,
    SQLLEN * StrLen_or_Ind) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLSpecialColumns)(HSTMT StatementHandle,
    SQLUSMALLINT IdentifierType,
    SQLTCHAR * CatalogName,
    SQLSMALLINT NameLength1,
    SQLTCHAR * SchemaName,
    SQLSMALLINT NameLength2,
    SQLTCHAR * TableName,
    SQLSMALLINT NameLength3,
    SQLUSMALLINT Scope,
    SQLUSMALLINT Nullable) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLStatistics)(HSTMT StatementHandle,
    SQLTCHAR * CatalogName,
    SQLSMALLINT NameLength1,
    SQLTCHAR * SchemaName,
    SQLSMALLINT NameLength2,
    SQLTCHAR * TableName,
    SQLSMALLINT NameLength3,
    SQLUSMALLINT Unique,
    SQLUSMALLINT Reserved) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLColumnPrivileges)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szTableName,
    SQLSMALLINT cbTableName,
    SQLTCHAR * szColumnName,
    SQLSMALLINT cbColumnName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


SQLRETURN SQL_API SQLDescribeParam(
    SQLHSTMT        StatementHandle,
    SQLUSMALLINT    ParameterNumber,
    SQLSMALLINT *   DataTypePtr,
    SQLULEN *       ParameterSizePtr,
    SQLSMALLINT *   DecimalDigitsPtr,
    SQLSMALLINT *   NullablePtr
) {
    LOG(__FUNCTION__);
    return impl::DescribeParam(
        StatementHandle,
        ParameterNumber,
        DataTypePtr,
        ParameterSizePtr,
        DecimalDigitsPtr,
        NullablePtr
    );
}


RETCODE SQL_API SQLExtendedFetch(HSTMT hstmt,
    SQLUSMALLINT fFetchType,
    SQLLEN irow,
#if defined(WITH_UNIXODBC) && (SIZEOF_LONG != 8)
    SQLROWSETSIZE * pcrow,
#else
    SQLULEN * pcrow,
#endif /* WITH_UNIXODBC */
    SQLUSMALLINT * rgfRowStatus) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLForeignKeys)(HSTMT hstmt,
    SQLTCHAR * szPkCatalogName,
    SQLSMALLINT cbPkCatalogName,
    SQLTCHAR * szPkSchemaName,
    SQLSMALLINT cbPkSchemaName,
    SQLTCHAR * szPkTableName,
    SQLSMALLINT cbPkTableName,
    SQLTCHAR * szFkCatalogName,
    SQLSMALLINT cbFkCatalogName,
    SQLTCHAR * szFkSchemaName,
    SQLSMALLINT cbFkSchemaName,
    SQLTCHAR * szFkTableName,
    SQLSMALLINT cbFkTableName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLPrimaryKeys)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szTableName,
    SQLSMALLINT cbTableName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLProcedureColumns)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szProcName,
    SQLSMALLINT cbProcName,
    SQLTCHAR * szColumnName,
    SQLSMALLINT cbColumnName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLProcedures)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szProcName,
    SQLSMALLINT cbProcName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLSetPos(HSTMT hstmt, SQLSETPOSIROW irow, SQLUSMALLINT fOption, SQLUSMALLINT fLock) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLTablePrivileges)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szTableName,
    SQLSMALLINT cbTableName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


SQLRETURN SQL_API SQLBindParameter(
    SQLHSTMT        StatementHandle,
    SQLUSMALLINT    ParameterNumber,
    SQLSMALLINT     InputOutputType,
    SQLSMALLINT     ValueType,
    SQLSMALLINT     ParameterType,
    SQLULEN         ColumnSize,
    SQLSMALLINT     DecimalDigits,
    SQLPOINTER      ParameterValuePtr,
    SQLLEN          BufferLength,
    SQLLEN *        StrLen_or_IndPtr
) {
    LOG(__FUNCTION__);
    return impl::BindParameter(
        StatementHandle,
        ParameterNumber,
        InputOutputType,
        ValueType,
        ParameterType,
        ColumnSize,
        DecimalDigits,
        ParameterValuePtr,
        BufferLength,
        StrLen_or_IndPtr
    );
}


/*
RETCODE SQL_API
SQLBulkOperations(
     SQLHSTMT       StatementHandle,
     SQLUSMALLINT   Operation)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}*/


RETCODE SQL_API SQLCancelHandle(SQLSMALLINT HandleType, SQLHANDLE Handle) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLCompleteAsync(SQLSMALLINT HandleType, SQLHANDLE Handle, RETCODE * AsyncRetCodePtr) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLCopyDesc(SQLHDESC SourceDescHandle, SQLHDESC TargetDescHandle) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLEndTran(SQLSMALLINT HandleType, SQLHANDLE Handle, SQLSMALLINT CompletionType) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLError)(SQLHENV hDrvEnv,
    SQLHDBC hDrvDbc,
    SQLHSTMT hDrvStmt,
    SQLTCHAR * szSqlState,
    SQLINTEGER * pfNativeError,
    SQLTCHAR * szErrorMsg,
    SQLSMALLINT nErrorMsgMax,
    SQLSMALLINT * pcbErrorMsg) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLGetDescField(SQLHDESC DescriptorHandle,
    SQLSMALLINT RecordNumber,
    SQLSMALLINT FieldIdentifier,
    SQLPOINTER Value,
    SQLINTEGER BufferLength,
    SQLINTEGER * StringLength) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLGetDescRec)(SQLHDESC DescriptorHandle,
    SQLSMALLINT RecordNumber,
    SQLTCHAR * Name,
    SQLSMALLINT BufferLength,
    SQLSMALLINT * StringLength,
    SQLSMALLINT * Type,
    SQLSMALLINT * SubType,
    SQLLEN * Length,
    SQLSMALLINT * Precision,
    SQLSMALLINT * Scale,
    SQLSMALLINT * Nullable) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLParamOptions(SQLHSTMT hDrvStmt, SQLULEN nRow, SQLULEN * pnRow) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLSetDescField(
    SQLHDESC DescriptorHandle, SQLSMALLINT RecordNumber, SQLSMALLINT FieldIdentifier, SQLPOINTER Value, SQLINTEGER BufferLength) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLSetDescRec(SQLHDESC hDescriptorHandle,
    SQLSMALLINT nRecordNumber,
    SQLSMALLINT nType,
    SQLSMALLINT nSubType,
    SQLLEN nLength,
    SQLSMALLINT nPrecision,
    SQLSMALLINT nScale,
    SQLPOINTER pData,
    SQLLEN * pnStringLength,
    SQLLEN * pnIndicator) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLSetScrollOptions(SQLHSTMT hDrvStmt, SQLUSMALLINT fConcurrency, SQLLEN crowKeyset, SQLUSMALLINT crowRowset) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLTransact(SQLHENV hDrvEnv, SQLHDBC hDrvDbc, UWORD nType) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


/*
 *	This function is used to cause the Driver Manager to
 *	call functions by number rather than name, which is faster.
 *	The ordinal value of this function must be 199 to have the
 *	Driver Manager do this.  Also, the ordinal values of the
 *	functions must match the value of fFunction in SQLGetFunctions()
 */
RETCODE SQL_API SQLDummyOrdinal(void) {
#if defined(_win_)
    // TODO (artpaul) implement SQLGetFunctions
    return SQL_ERROR;
#else
    return SQL_ERROR;
#endif
}

} // extern "C"
