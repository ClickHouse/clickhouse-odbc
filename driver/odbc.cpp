#include "connection.h"
#include "diagnostics.h"
#include "environment.h"
#include "log/log.h"
#include "result_set.h"
#include "statement.h"
#include "string_ref.h"
#include "type_parser.h"
#include "utils.h"
#include "scope_guard.h"

#include <stdio.h>
//#include <malloc.h>
#include <string.h>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>

/** Functions from the ODBC interface can not directly call other functions.
  * Because not a function from this library will be called, but a wrapper from the driver manager,
  * which can work incorrectly, being called from within another function.
  * Wrong - because driver manager wraps all handle in its own,
  * which already have other addresses.
  */

extern "C" {


/// Description: https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlconnect-function
RETCODE SQL_API FUNCTION_MAYBE_W(SQLConnect)(HDBC connection_handle,
    SQLTCHAR * dsn,
    SQLSMALLINT dsn_size,
    SQLTCHAR * user,
    SQLSMALLINT user_size,
    SQLTCHAR * password,
    SQLSMALLINT password_size)
{
    // LOG(__FUNCTION__ << " dsn_size=" << dsn_size << " dsn=" << dsn << " user_size=" << user_size << " user=" << user << " password_size=" << password_size << " password=" << password);

    return doWith<Connection>(connection_handle, [&](Connection & connection) {

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
    SQLUSMALLINT driver_completion)
{
    LOG(__FUNCTION__ << " connection_str_in=" << connection_str_in << " : " << connection_str_in_size << " connection_str_out=" << connection_str_out << " : " << connection_str_out_max_size);

    return doWith<Connection>(connection_handle, [&](Connection & connection) {
        connection.init(stringFromSQLSymbols(connection_str_in, connection_str_in_size));
        // Copy complete connection string.
        fillOutputPlatformString(
            connection.connectionString(), connection_str_out, connection_str_out_max_size, connection_str_out_size, false);
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLPrepare)(HSTMT statement_handle, SQLTCHAR * statement_text, SQLINTEGER statement_text_size)
{
    LOG(__FUNCTION__ << " statement_text_size=" << statement_text_size << " statement_text=" << statement_text);

    return doWith<Statement>(statement_handle, [&](Statement & statement) {
        const std::string & query = stringFromSQLSymbols(statement_text, statement_text_size);
        if (!statement.isEmpty())
            throw std::runtime_error("Prepare called, but statement query is not empty.");
        if (query.empty())
            throw std::runtime_error("Prepare called with empty query.");

        statement.prepareQuery(query);

        LOG("query(" << query.size() << ") = [" << query << "]");

        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLExecute(HSTMT statement_handle)
{
    LOG(__FUNCTION__);

    return doWith<Statement>(statement_handle, [&](Statement & statement) {
        statement.sendRequest();
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLExecDirect)(HSTMT statement_handle, SQLTCHAR * statement_text, SQLINTEGER statement_text_size)
{
    //LOG(__FUNCTION__);

    return doWith<Statement>(statement_handle, [&](Statement & statement) {
        const std::string & query = stringFromSQLSymbols(statement_text, statement_text_size);

        LOG(__FUNCTION__ << " statement_text_size=" << statement_text_size << " statement_text=" << query );

        if (!statement.isEmpty())
        {
            if (!statement.isPrepared()) {
                throw std::runtime_error("ExecDirect called, but statement query is not empty.");
            }
            else if (statement.getQuery() != query) {
                throw std::runtime_error("ExecDirect called, but statement query is not equal to prepared. [" + statement.getQuery() + "] != [" + query + "]...");
            }
        }
        else
        {
            if (query.empty())
                throw std::runtime_error("ExecDirect called with empty query.");

            statement.prepareQuery(query);
        }

        //LOG(query);
        statement.sendRequest();
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLNumResultCols(HSTMT statement_handle, SQLSMALLINT * column_count)
{
    LOG(__FUNCTION__);

    return doWith<Statement>(statement_handle, [&](Statement & statement) {
        *column_count = (SQLSMALLINT)statement.result.getNumColumns();
        LOG(*column_count);
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
        out_num_value)
{
    LOG(__FUNCTION__ << "(col=" << column_number << ", field=" << field_identifier << ")");

    return doWith<Statement>(statement_handle, [&](Statement & statement) -> RETCODE {
        if (column_number < 1 || column_number > statement.result.getNumColumns()) {
            LOG(__FUNCTION__ << ": Column number is out of range.");
            throw SqlException("Column number is out of range.", "07009");
        }

        size_t column_idx = column_number - 1;

        SQLLEN num_value = 0;
        std::string str_value;

        const ColumnInfo & column_info = statement.result.getColumnInfo(column_idx);
        const TypeInfo & type_info = statement.connection.environment.types_info.at(column_info.type_without_parameters);

        switch (field_identifier)
        {
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
                num_value = statement.result.getNumColumns();
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
                    num_value = std::min<int32_t>(statement.connection.stringmaxlength, column_info.fixed_size ? column_info.fixed_size : column_info.display_size);
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
                    num_value = std::min<int32_t>(statement.connection.stringmaxlength, column_info.fixed_size ? column_info.fixed_size : column_info.display_size) * SIZEOF_CHAR;
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
    SQLSMALLINT * out_is_nullable)
{

    return doWith<Statement>(statement_handle, [&](Statement & statement) {
        if (column_number < 1 || column_number > statement.result.getNumColumns())
            throw std::runtime_error("Column number is out of range.");

        size_t column_idx = column_number - 1;

        const ColumnInfo & column_info = statement.result.getColumnInfo(column_idx);
        const TypeInfo & type_info = statement.connection.environment.types_info.at(column_info.type_without_parameters);

        LOG(__FUNCTION__ << " column_number=" << column_number << "name=" << column_info.name <<" type=" << type_info.sql_type << " size=" << type_info.column_size << " nullable=" << column_info.is_nullable);

        if (out_type)
            *out_type = type_info.sql_type;
        if (out_column_size)
            *out_column_size = std::min<int32_t>(statement.connection.stringmaxlength, column_info.fixed_size ? column_info.fixed_size : type_info.column_size);
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
    SQLLEN * out_value_size_or_indicator)
{
    LOG(__FUNCTION__ << " column_or_param_number=" << column_or_param_number << " target_type=" << target_type);
#ifndef NDEBUG
    SCOPE_EXIT({ LOG("impl_SQLGetData finish."); }); // for timing only
#endif

    return doWith<Statement>(statement_handle, [&](Statement & statement) -> RETCODE {
        if (column_or_param_number < 1 || column_or_param_number > statement.result.getNumColumns()) {
            LOG(__FUNCTION__ << ": Column number is out of range (throw)." << column_or_param_number);
            throw std::runtime_error("Column number is out of range.");
        }

        size_t column_idx = column_or_param_number - 1;

        const Field & field = statement.current_row.data[column_idx];

        LOG("column: " << column_idx << ", target_type: " << target_type << ", out_value_max_size: " << out_value_max_size << " null=" << field.is_null << " data=" << field.data);

        if (field.is_null)
            return fillOutputNULL(out_value, out_value_max_size, out_value_size_or_indicator);

        switch (target_type)
        {
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
impl_SQLFetch(HSTMT statement_handle)
{
    LOG(__FUNCTION__);
#ifndef NDEBUG
    SCOPE_EXIT({ LOG("impl_SQLFetch finish."); }); // for timing only
#endif

    return doWith<Statement>(statement_handle, [&](Statement & statement) -> RETCODE {
        if (!statement.fetchRow())
            return SQL_NO_DATA;

        // LOG("impl_SQLFetch statement.bindings.size()=" << statement.bindings.size());

        auto res = SQL_SUCCESS;

        for (auto & col_num_binding : statement.bindings)
        {
            auto code = impl_SQLGetData(statement_handle,
                col_num_binding.first,
                col_num_binding.second.target_type,
                col_num_binding.second.out_value,
                col_num_binding.second.out_value_max_size,
                col_num_binding.second.out_value_size_or_indicator);

            if (code == SQL_SUCCESS_WITH_INFO)
                res = code;
            else if (code != SQL_SUCCESS)
                return code;
        }

        return res;
    });
}


RETCODE SQL_API SQLFetch(HSTMT statement_handle)
{
    return impl_SQLFetch(statement_handle);
}


RETCODE SQL_API SQLFetchScroll(HSTMT statement_handle, SQLSMALLINT orientation, SQLLEN offset)
{
    LOG(__FUNCTION__);

    return doWith<Statement>(statement_handle, [&](Statement & statement) -> RETCODE {
        if (orientation != SQL_FETCH_NEXT)
            throw std::runtime_error("Fetch type out of range"); /// TODO sqlstate = HY106

        return impl_SQLFetch(statement_handle);
    });
}


RETCODE SQL_API SQLGetData(HSTMT statement_handle,
    SQLUSMALLINT column_or_param_number,
    SQLSMALLINT target_type,
    PTR out_value,
    SQLLEN out_value_max_size,
    SQLLEN * out_value_size_or_indicator)
{
    return impl_SQLGetData(
        statement_handle, column_or_param_number, target_type, out_value, out_value_max_size, out_value_size_or_indicator);
}


RETCODE SQL_API SQLBindCol(HSTMT statement_handle,
    SQLUSMALLINT column_number,
    SQLSMALLINT target_type,
    PTR out_value,
    SQLLEN out_value_max_size,
    SQLLEN * out_value_size_or_indicator)
{
    LOG(__FUNCTION__);

    return doWith<Statement>(statement_handle, [&](Statement & statement) {
        if (column_number < 1 || column_number > statement.result.getNumColumns())
            throw SqlException("Column number " + std::to_string(column_number) + " is out of range: " + std::to_string(statement.result.getNumColumns()), "07009");
        if (out_value_max_size < 0)
            throw SqlException("Invalid string or buffer length", "HY090");

        // Unbinding column
        if (out_value_size_or_indicator == nullptr)
        {
            statement.bindings.erase(column_number);
            return SQL_SUCCESS;
        }

        if (target_type == SQL_C_DEFAULT)
        {
            target_type = statement.getTypeInfo(statement.result.getColumnInfo(column_number - 1).type_without_parameters).sql_type;
        }

        Binding binding;
        binding.target_type = target_type;
        binding.out_value = out_value;
        binding.out_value_max_size = out_value_max_size;
        binding.out_value_size_or_indicator = out_value_size_or_indicator;

        statement.bindings[column_number] = binding;

        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLRowCount(HSTMT statement_handle, SQLLEN * out_row_count)
{
    LOG(__FUNCTION__);

    return doWith<Statement>(statement_handle, [&](Statement & statement) {
        if (out_row_count) {
            *out_row_count = statement.result.getNumRows();
            LOG("getNumRows=" << *out_row_count);
        }
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLMoreResults(HSTMT hstmt)
{
    LOG(__FUNCTION__);
    // TODO (artpaul) MS Excel call this function.
    return SQL_NO_DATA;
}


RETCODE SQL_API SQLDisconnect(HDBC connection_handle)
{
    LOG(__FUNCTION__);

    return doWith<Connection>(connection_handle, [&](Connection & connection) {
        connection.session->reset();
        return SQL_SUCCESS;
    });
}


RETCODE
impl_SQLGetDiagRec(SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLTCHAR * out_sqlstate,
    SQLINTEGER * out_native_error_code,
    SQLTCHAR * out_mesage,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size)
{
    LOG(__FUNCTION__ << " handle_type: " << handle_type << ", record_number: " << record_number << ", out_message_max_size: " << out_message_max_size);

    if (nullptr == handle)
        return SQL_INVALID_HANDLE;

    if (record_number <= 0 || out_message_max_size < 0)
        return SQL_ERROR;

    if (record_number > 1)
        return SQL_NO_DATA;

    DiagnosticRecord * diagnostic_record = nullptr;
    switch (handle_type)
    {
        case SQL_HANDLE_ENV:
            diagnostic_record = &reinterpret_cast<Environment *>(handle)->diagnostic_record;
            break;
        case SQL_HANDLE_DBC:
            diagnostic_record = &reinterpret_cast<Connection *>(handle)->diagnostic_record;
            break;
        case SQL_HANDLE_STMT:
            diagnostic_record = &reinterpret_cast<Statement *>(handle)->diagnostic_record;
            break;
        case SQL_HANDLE_DESC:
            // TODO (artpaul) implement
            return SQL_NO_DATA;
        default:
            return SQL_ERROR;
    }

    if (diagnostic_record->native_error_code == 0)
        return SQL_NO_DATA;

    /// The five-letter SQLSTATE and the trailing zero.
    if (out_sqlstate)
    {
        size_t size = 6;
        size_t written = 0;
        fillOutputPlatformString(diagnostic_record->sql_state, out_sqlstate, size, &written, true);
    }
    if (out_native_error_code)
        *out_native_error_code = diagnostic_record->native_error_code;

    return fillOutputPlatformString(diagnostic_record->message, out_mesage, out_message_max_size, out_message_size, false);
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLGetDiagRec)(SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLTCHAR * out_sqlstate,
    SQLINTEGER * out_native_error_code,
    SQLTCHAR * out_mesage,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size)
{
    return impl_SQLGetDiagRec(
        handle_type, handle, record_number, out_sqlstate, out_native_error_code, out_mesage, out_message_max_size, out_message_size);
}


/// Description: https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdiagfield-function
RETCODE SQL_API SQLGetDiagField(SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLSMALLINT field_id,
    SQLPOINTER out_mesage,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size)
{
    LOG(__FUNCTION__);

    return impl_SQLGetDiagRec(handle_type,
        handle,
        record_number,
        nullptr,
        nullptr,
        reinterpret_cast<SQLTCHAR *>(out_mesage),
        out_message_max_size,
        out_message_size);
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
    SQLSMALLINT table_type_length)
{
    LOG(__FUNCTION__);

    // TODO (artpaul) Take statement.getMetatadaId() into account.
    return doWith<Statement>(statement_handle, [&](Statement & statement) {
        const std::string catalog = stringFromSQLSymbols(catalog_name, catalog_name_length);

        std::stringstream query;

        // Get a list of all tables in all databases.
        if (catalog_name != nullptr && catalog == SQL_ALL_CATALOGS && !schema_name && !table_name && !table_type)
        {
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
        else if (!catalog_name && !schema_name && !table_name && !table_type)
        {
            query << "SELECT"
                     " database AS TABLE_CAT"
                     ", '' AS TABLE_SCHEM"
                     ", name AS TABLE_NAME"
                     ", 'TABLE' AS TABLE_TYPE"
                     ", '' AS REMARKS"
                     " FROM system.tables"
                     " WHERE (database == '";
            query << statement.connection.getDatabase() << "')";
            query << " ORDER BY TABLE_TYPE, TABLE_CAT, TABLE_SCHEM, TABLE_NAME";
        }
        // Get a list of databases on the current connection's server.
        else if (!catalog.empty() && schema_name != nullptr && schema_name_length == 0 && table_name != nullptr && table_name_length == 0)
        {
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
        }
        else
        {
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

        statement.setQuery(query.str());
        statement.sendRequest();
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
    SQLSMALLINT column_name_length)
{
    LOG(__FUNCTION__);

    class ColumnsMutator : public IResultMutator
    {
    public:
        ColumnsMutator(Environment * env_) : env(env_) {}

        void UpdateColumnInfo(std::vector<ColumnInfo> * columns_info) override
        {
            columns_info->at(4).name = "Int16";
            columns_info->at(4).type_without_parameters = "Int16";
        }

        void UpdateRow(const std::vector<ColumnInfo> & columns_info, Row * row) override
        {
            ColumnInfo type_column;

            {
                TypeAst ast;
                if (TypeParser(row->data.at(4).data).parse(&ast))
                {
                    assignTypeInfo(ast, &type_column);
                }
                else
                {
                    // Interprete all unknown types as String.
                    type_column.type_without_parameters = "String";
                }
            }

            const TypeInfo & type_info = env->types_info.at(type_column.type_without_parameters);

            row->data.at(4).data = std::to_string(type_info.sql_type);
            row->data.at(5).data = type_info.sql_type_name;
            row->data.at(6).data = std::to_string(type_info.column_size);
            row->data.at(13).data = std::to_string(type_info.sql_type);
            row->data.at(15).data = std::to_string(type_info.octet_length);
        }

    private:
        Environment * const env;
    };

    return doWith<Statement>(statement_handle, [&](Statement & statement) {
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
        if (s.length() > 0)
        {
            query << " AND TABLE_CAT LIKE '" << s << "'";
        }
        else
        {
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

        statement.setQuery(query.str());
        statement.sendRequest(IResultMutatorPtr(new ColumnsMutator(&statement.connection.environment)));
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLGetTypeInfo(HSTMT statement_handle, SQLSMALLINT type)
{
    LOG(__FUNCTION__ << "(type = " << type << ")");

    return doWith<Statement>(statement_handle, [&](Statement & statement) {
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

        for (const auto & name_info : statement.connection.environment.types_info)
        {
            add_query_for_type(name_info.first, name_info.second);
        }

        // TODO (artpaul) check current version of ODBC.
        //
        //      In ODBC 3.x, the SQL date, time, and timestamp data types
        //      are SQL_TYPE_DATE, SQL_TYPE_TIME, and SQL_TYPE_TIMESTAMP, respectively;
        //      in ODBC 2.x, the data types are SQL_DATE, SQL_TIME, and SQL_TIMESTAMP.
        {
            auto info = statement.connection.environment.types_info.at("Date");
            info.sql_type = SQL_DATE;
            add_query_for_type("Date", info);
        }

        {
            auto info = statement.connection.environment.types_info.at("DateTime");
            info.sql_type = SQL_TIMESTAMP;
            add_query_for_type("DateTime", info);
        }

        query << ") ORDER BY DATA_TYPE";

        if (first)
            query.str("SELECT 1 WHERE 0");

        statement.setQuery(query.str());
        statement.sendRequest();
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API SQLNumParams(HSTMT statement_handle, SQLSMALLINT * out_params_count)
{
    LOG(__FUNCTION__);

    return doWith<Statement>(statement_handle, [&](Statement & statement) {
        *out_params_count = 0;
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLNativeSql)(HDBC connection_handle,
    SQLTCHAR * query,
    SQLINTEGER query_length,
    SQLTCHAR * out_query,
    SQLINTEGER out_query_max_length,
    SQLINTEGER * out_query_length)
{
    LOG(__FUNCTION__);

    return doWith<Connection>(connection_handle, [&](Connection & connection) {
        std::string query_str = stringFromSQLSymbols(query, query_length);
        return fillOutputPlatformString(query_str, out_query, out_query_max_length, out_query_length, false);
    });
}


RETCODE SQL_API SQLCloseCursor(HSTMT statement_handle)
{
    LOG(__FUNCTION__);

    return doWith<Statement>(statement_handle, [&](Statement & statement) -> RETCODE {
        statement.reset();
        return SQL_SUCCESS;
    });
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLBrowseConnect)(HDBC connection_handle,
    SQLTCHAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLTCHAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT * pcbConnStrOut)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


/// Not implemented.


RETCODE SQL_API SQLCancel(HSTMT StatementHandle)
{
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
    SQLSMALLINT * NameLength2)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLGetCursorName)(HSTMT StatementHandle, SQLTCHAR * CursorName, SQLSMALLINT BufferLength, SQLSMALLINT * NameLength)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


/// This function can be implemented in the driver manager.
RETCODE SQL_API SQLGetFunctions(HDBC connection_handle, SQLUSMALLINT FunctionId, SQLUSMALLINT * Supported)
{
    LOG(__FUNCTION__ << ":" << __LINE__ << " "
                     << " id=" << FunctionId << " ptr=" << Supported);
    return doWith<Connection>(connection_handle, [&](Connection & connection) -> RETCODE {
        if (FunctionId == SQL_API_ODBC3_ALL_FUNCTIONS)
        {
            memset(Supported, 0, sizeof(Supported[0]) * SQL_API_ODBC3_ALL_FUNCTIONS_SIZE);
#define SET_EXISTS(x) Supported[(x) >> 4] |= (1 << ((x)&0xF))
            // #define CLR_EXISTS(x) Supported[(x) >> 4] &= ~(1 << ((x) & 0xF))
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
            // CLR_EXISTS(SQL_API_SQLSETPARAM);
            // CLR_EXISTS(SQL_API_SQLSPECIALCOLUMNS);
            // CLR_EXISTS(SQL_API_SQLSTATISTICS);
            // CLR_EXISTS(SQL_API_SQLCOLUMNPRIVILEGES);

            /// TODO: more here, but all not implemented
#undef SET_EXISTS
            // #undef CLR_EXISTS
            return SQL_SUCCESS;
        }
        else if (FunctionId == SQL_API_ALL_FUNCTIONS)
        {
            //memset(Supported, 0, sizeof(Supported[0]) * 100);
            return SQL_ERROR;
        }
        else
        {
        /*
		switch (FunctionId)
		{
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
}


RETCODE SQL_API SQLParamData(HSTMT StatementHandle, PTR * Value)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLPutData(HSTMT StatementHandle, PTR Data, SQLLEN StrLen_or_Ind)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLSetCursorName)(HSTMT StatementHandle, SQLTCHAR * CursorName, SQLSMALLINT NameLength)
{
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
    SQLLEN * StrLen_or_Ind)
{
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
    SQLUSMALLINT Nullable)
{
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
    SQLUSMALLINT Reserved)
{
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
    SQLSMALLINT cbColumnName)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLDescribeParam(
    HSTMT hstmt, SQLUSMALLINT ipar, SQLSMALLINT * pfSqlType, SQLULEN * pcbParamDef, SQLSMALLINT * pibScale, SQLSMALLINT * pfNullable)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLExtendedFetch(HSTMT hstmt,
    SQLUSMALLINT fFetchType,
    SQLLEN irow,
#if defined(WITH_UNIXODBC) && (SIZEOF_LONG != 8)
    SQLROWSETSIZE * pcrow,
#else
    SQLULEN * pcrow,
#endif /* WITH_UNIXODBC */
    SQLUSMALLINT * rgfRowStatus)
{
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
    SQLSMALLINT cbFkTableName)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLPrimaryKeys)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szTableName,
    SQLSMALLINT cbTableName)
{
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
    SQLSMALLINT cbColumnName)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLProcedures)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szProcName,
    SQLSMALLINT cbProcName)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLSetPos(HSTMT hstmt, SQLSETPOSIROW irow, SQLUSMALLINT fOption, SQLUSMALLINT fLock)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API FUNCTION_MAYBE_W(SQLTablePrivileges)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szTableName,
    SQLSMALLINT cbTableName)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLBindParameter(HSTMT hstmt,
    SQLUSMALLINT ipar,
    SQLSMALLINT fParamType,
    SQLSMALLINT fCType,
    SQLSMALLINT fSqlType,
    SQLULEN cbColDef,
    SQLSMALLINT ibScale,
    PTR rgbValue,
    SQLLEN cbValueMax,
    SQLLEN * pcbValue)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
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


RETCODE SQL_API SQLCancelHandle(SQLSMALLINT HandleType, SQLHANDLE Handle)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLCompleteAsync(SQLSMALLINT HandleType, SQLHANDLE Handle, RETCODE * AsyncRetCodePtr)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLCopyDesc(SQLHDESC SourceDescHandle, SQLHDESC TargetDescHandle)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLEndTran(SQLSMALLINT HandleType, SQLHANDLE Handle, SQLSMALLINT CompletionType)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLError(SQLHENV hDrvEnv,
    SQLHDBC hDrvDbc,
    SQLHSTMT hDrvStmt,
    SQLTCHAR * szSqlState,
    SQLINTEGER * pfNativeError,
    SQLTCHAR * szErrorMsg,
    SQLSMALLINT nErrorMsgMax,
    SQLSMALLINT * pcbErrorMsg)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLGetDescField(SQLHDESC DescriptorHandle,
    SQLSMALLINT RecordNumber,
    SQLSMALLINT FieldIdentifier,
    SQLPOINTER Value,
    SQLINTEGER BufferLength,
    SQLINTEGER * StringLength)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLGetDescRec(SQLHDESC DescriptorHandle,
    SQLSMALLINT RecordNumber,
    SQLTCHAR * Name,
    SQLSMALLINT BufferLength,
    SQLSMALLINT * StringLength,
    SQLSMALLINT * Type,
    SQLSMALLINT * SubType,
    SQLLEN * Length,
    SQLSMALLINT * Precision,
    SQLSMALLINT * Scale,
    SQLSMALLINT * Nullable)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLParamOptions(SQLHSTMT hDrvStmt, SQLULEN nRow, SQLULEN * pnRow)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLSetDescField(
    SQLHDESC DescriptorHandle, SQLSMALLINT RecordNumber, SQLSMALLINT FieldIdentifier, SQLPOINTER Value, SQLINTEGER BufferLength)
{
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
    SQLLEN * pnIndicator)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLSetScrollOptions(SQLHSTMT hDrvStmt, SQLUSMALLINT fConcurrency, SQLLEN crowKeyset, SQLUSMALLINT crowRowset)
{
    LOG(__FUNCTION__);
    return SQL_ERROR;
}


RETCODE SQL_API SQLTransact(SQLHENV hDrvEnv, SQLHDBC hDrvDbc, UWORD nType)
{
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
RETCODE SQL_API SQLDummyOrdinal(void)
{
#if defined(_win_)
    // TODO (artpaul) implement SQLGetFunctions
    return SQL_ERROR;
#else
    return SQL_ERROR;
#endif
}
}
