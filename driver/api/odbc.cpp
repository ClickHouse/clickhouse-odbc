#include "driver/utils/utils.h"
#include "driver/utils/scope_guard.h"
#include "driver/utils/string_ref.h"
#include "driver/utils/type_parser.h"
#include "driver/attributes.h"
#include "driver/diagnostics.h"
#include "driver/driver.h"
#include "driver/environment.h"
#include "driver/connection.h"
#include "driver/descriptor.h"
#include "driver/statement.h"
#include "driver/result_set.h"

#include <Poco/Net/HTTPClientSession.h>

#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>

#include <cstdio>
#include <cstring>

/** Functions from the ODBC interface can not directly call other functions.
  * Because not a function from this library will be called, but a wrapper from the driver manager,
  * which can work incorrectly, being called from within another function.
  * Wrong - because driver manager wraps all handle in its own,
  * which already have other addresses.
  * The actual implementation bodies are also moved out of from the ODBC interface calls,
  * to be out of extern "C" section, so that C++ features like generic lambdas, templates, are allowed,
  * for example, by MSVC.
  */

namespace { namespace impl {

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
            fillOutputString<SQLTCHAR>(record.template getAttrAs<std::string>(SQL_DIAG_SQLSTATE), out_sqlstate, size, &written, false);
        }

        if (out_native_error_code != nullptr) {
            *out_native_error_code = record.template getAttrAs<SQLINTEGER>(SQL_DIAG_NATIVE);
        }

        return fillOutputString<SQLTCHAR>(record.template getAttrAs<std::string>(SQL_DIAG_MESSAGE_TEXT), out_message, out_message_max_size, out_message_size, false);
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
        if (getObjectHandleType<decltype(object)>() != SQL_HANDLE_STMT) {
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
        return fillOutputString<SQLTCHAR>(record.template getAttrAs<std::string>(NAME), out_message, out_message_max_size, out_message_size, true);

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
        auto & apd_desc = statement.getEffectiveDescriptor(SQL_ATTR_APP_PARAM_DESC);
        auto & ipd_desc = statement.getEffectiveDescriptor(SQL_ATTR_IMP_PARAM_DESC);

        const auto apd_record_count = apd_desc.getRecordCount();
        const auto ipd_record_count = ipd_desc.getRecordCount();

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

            apd_record.setAttr(SQL_DESC_OCTET_LENGTH, buffer_length);
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

    return CALL_WITH_HANDLE(handle, func);
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

    return CALL_WITH_HANDLE(handle, func);
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

        // Process header fields first, withour running cheecks on record number.
        switch (FieldIdentifier) {

#define CASE_FIELD_NUM(NAME, TYPE) \
            case NAME: return fillOutputPOD<TYPE>(descriptor.getAttrAs<TYPE>(NAME), ValuePtr, StringLengthPtr);

#define CASE_FIELD_NUM_DEF(NAME, TYPE, DEFAULT) \
            case NAME: return fillOutputPOD<TYPE>(descriptor.getAttrAs<TYPE>(NAME, DEFAULT), ValuePtr, StringLengthPtr);

            CASE_FIELD_NUM     ( SQL_DESC_ALLOC_TYPE,         SQLSMALLINT                       );
            CASE_FIELD_NUM_DEF ( SQL_DESC_ARRAY_SIZE,         SQLULEN,       1                  );
            CASE_FIELD_NUM     ( SQL_DESC_ARRAY_STATUS_PTR,   SQLUSMALLINT *                    );
            CASE_FIELD_NUM     ( SQL_DESC_BIND_OFFSET_PTR,    SQLLEN *                          );
            CASE_FIELD_NUM_DEF ( SQL_DESC_BIND_TYPE,          SQLUINTEGER,   SQL_BIND_BY_COLUMN );
            CASE_FIELD_NUM     ( SQL_DESC_COUNT,              SQLSMALLINT                       );
            CASE_FIELD_NUM     ( SQL_DESC_ROWS_PROCESSED_PTR, SQLULEN *                         );
                
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
            case NAME: return fillOutputString<SQLTCHAR>(record.getAttrAs<std::string>(NAME), ValuePtr, BufferLength, StringLengthPtr, true);

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

    return CALL_WITH_HANDLE(DescriptorHandle, func);
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

        return fillOutputString<SQLTCHAR>(record.getAttrAs<std::string>(SQL_DESC_NAME), Name, BufferLength, StringLengthPtr, false);
    };

    return CALL_WITH_HANDLE(DescriptorHandle, func);
}

SQLRETURN SetDescField(
    SQLHDESC      DescriptorHandle,
    SQLSMALLINT   RecNumber,
    SQLSMALLINT   FieldIdentifier,
    SQLPOINTER    ValuePtr,
    SQLINTEGER    BufferLength
) noexcept {
    auto func = [&] (Descriptor & descriptor) -> SQLRETURN {

        // Process header fields first, withour running cheecks on record number.
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

    return CALL_WITH_HANDLE(DescriptorHandle, func);
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

    return CALL_WITH_HANDLE(DescriptorHandle, func);
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

        auto rc = CALL_WITH_HANDLE_SKIP_DIAG(SourceDescHandle, func);

        if (ex)
            std::rethrow_exception(ex);

        if (rc == SQL_INVALID_HANDLE)
            throw SqlException("Invalid attribute value", "HY024");

        return rc;
    };

    return CALL_WITH_HANDLE(TargetDescHandle, func);
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

SQLRETURN GetData(
    HSTMT statement_handle,
    SQLUSMALLINT column_or_param_number,
    SQLSMALLINT target_type,
    PTR out_value,
    SQLLEN out_value_max_size,
    SQLLEN * out_value_size_or_indicator
) noexcept {
    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) -> SQLRETURN {
        if (!statement.hasResultSet())
            throw SqlException("Column info is not available", "07009");

        if (column_or_param_number < 1 || column_or_param_number > statement.getNumColumns())
            throw SqlException("Column number " + std::to_string(column_or_param_number) + " is out of range: 1.." +
                std::to_string(statement.getNumColumns()), "07009");

        if (!statement.hasCurrentRow())
            throw SqlException("Invalid cursor state", "24000");

        const auto column_idx = column_or_param_number - 1;

        const Field & field = statement.getCurrentRow().data[column_idx];

        LOG("column: " << column_idx << ", target_type: " << target_type << ", out_value_max_size: " << out_value_max_size
                       << " null=" << field.is_null << " data=" << field.data);


        // TODO: revisit the code, use descriptors for all cases.

        
        SQLINTEGER desc_type = SQL_ATTR_APP_ROW_DESC;

//      if (target_type == SQL_APD_TYPE) {
//          desc_type = SQL_ATTR_APP_PARAM_DESC;
//          throw SqlException("Unable to read parameter data using SQLGetData");
//      }

        if (
            target_type == SQL_ARD_TYPE ||
//          target_type == SQL_APD_TYPE ||
            target_type == SQL_C_DEFAULT
        ) {
            auto & desc = statement.getEffectiveDescriptor(desc_type);
            auto & record = desc.getRecord(column_or_param_number, desc_type);

            target_type = record.getAttrAs<SQLSMALLINT>(SQL_DESC_CONCISE_TYPE, SQL_C_DEFAULT);
        }

        if (field.is_null)
            return fillOutputNULL(out_value, out_value_max_size, out_value_size_or_indicator);

        switch (target_type) {
            case SQL_C_BINARY:
                return fillOutputBuffer(field.data.data(), field.data.size(), out_value, out_value_max_size, out_value_size_or_indicator);

            case SQL_C_CHAR:
                return fillOutputString<SQLCHAR>(field.data, out_value, out_value_max_size, out_value_size_or_indicator, true);

            case SQL_C_WCHAR:
                return fillOutputString<SQLWCHAR>(field.data, out_value, out_value_max_size, out_value_size_or_indicator, true);

            case SQL_C_TINYINT:
            case SQL_C_STINYINT:
                return fillOutputPOD<SQLSCHAR>(field.getInt(), out_value, out_value_size_or_indicator);

            case SQL_C_UTINYINT:
            case SQL_C_BIT:
                return fillOutputPOD<SQLCHAR>(field.getUInt(), out_value, out_value_size_or_indicator);

            case SQL_C_SHORT:
            case SQL_C_SSHORT:
                return fillOutputPOD<SQLSMALLINT>(field.getInt(), out_value, out_value_size_or_indicator);

            case SQL_C_USHORT:
                return fillOutputPOD<SQLUSMALLINT>(field.getUInt(), out_value, out_value_size_or_indicator);

            case SQL_C_LONG:
            case SQL_C_SLONG:
                return fillOutputPOD<SQLINTEGER>(field.getInt(), out_value, out_value_size_or_indicator);

            case SQL_C_ULONG:
                return fillOutputPOD<SQLUINTEGER>(field.getUInt(), out_value, out_value_size_or_indicator);

            case SQL_C_SBIGINT:
                return fillOutputPOD<SQLBIGINT>(field.getInt(), out_value, out_value_size_or_indicator);

            case SQL_C_UBIGINT:
                return fillOutputPOD<SQLUBIGINT>(field.getUInt(), out_value, out_value_size_or_indicator);

            case SQL_C_FLOAT:
                return fillOutputPOD<SQLREAL>(field.getFloat(), out_value, out_value_size_or_indicator);

            case SQL_C_DOUBLE:
                return fillOutputPOD<SQLDOUBLE>(field.getDouble(), out_value, out_value_size_or_indicator);

            case SQL_C_GUID:
                return fillOutputPOD<SQLGUID>(field.getGUID(), out_value, out_value_size_or_indicator);

            case SQL_C_NUMERIC: {
                auto & desc = statement.getEffectiveDescriptor(desc_type);
                auto & record = desc.getRecord(column_or_param_number, desc_type);

                const std::int16_t precision = record.getAttrAs<SQLSMALLINT>(SQL_DESC_PRECISION, 38);
                const std::int16_t scale = record.getAttrAs<SQLSMALLINT>(SQL_DESC_SCALE, 0);

                return fillOutputPOD<SQL_NUMERIC_STRUCT>(field.getNumeric(precision, scale), out_value, out_value_size_or_indicator);
            }

            case SQL_C_DATE:
            case SQL_C_TYPE_DATE:
                return fillOutputPOD<SQL_DATE_STRUCT>(field.getDate(), out_value, out_value_size_or_indicator);

//          case SQL_C_TIME:
//          case SQL_C_TYPE_TIME:
//              return fillOutputPOD<SQL_TIME_STRUCT>(field.getTime(), out_value, out_value_size_or_indicator);

            case SQL_C_TIMESTAMP:
            case SQL_C_TYPE_TIMESTAMP:
                return fillOutputPOD<SQL_TIMESTAMP_STRUCT>(field.getDateTime(), out_value, out_value_size_or_indicator);

            default:
                throw SqlException("Restricted data type attribute violation", "07006");
        }
    });
}

SQLRETURN Fetch(
    HSTMT statement_handle
) noexcept {
    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) -> SQLRETURN {
        auto * rows_fetched_ptr = statement.getEffectiveDescriptor(SQL_ATTR_IMP_ROW_DESC).getAttrAs<SQLULEN *>(SQL_DESC_ROWS_PROCESSED_PTR, 0);

        if (rows_fetched_ptr)
            *rows_fetched_ptr = 0;

        if (!statement.hasResultSet())
            return SQL_NO_DATA;

        if (!statement.advanceToNextRow())
            return SQL_NO_DATA;

        if (rows_fetched_ptr)
            *rows_fetched_ptr = 1;

        auto res = SQL_SUCCESS;

        for (auto & col_num_binding : statement.bindings) {
            auto code = impl::GetData(statement_handle,
                col_num_binding.first,
                col_num_binding.second.c_type,
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

} } // namespace impl


extern "C" {

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLConnect)(HDBC connection_handle,
    SQLTCHAR * dsn,
    SQLSMALLINT dsn_size,
    SQLTCHAR * user,
    SQLSMALLINT user_size,
    SQLTCHAR * password,
    SQLSMALLINT password_size) {
    // LOG(__FUNCTION__ << " dsn_size=" << dsn_size << " dsn=" << dsn << " user_size=" << user_size << " user=" << user << " password_size=" << password_size << " password=" << password);

    return CALL_WITH_HANDLE(connection_handle, [&](Connection & connection) {
        const auto dsn_str = toUTF8(dsn, dsn_size);
        const auto user_str = toUTF8(user, user_size);
        const auto password_str = toUTF8(password, password_size);

        LOG(__FUNCTION__ << " dsn=" << dsn_str << " user=" << user_str << " pwd=" << password_str);

        connection.init(dsn_str, 0, user_str, password_str, "");
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLDriverConnect)(HDBC connection_handle,
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
        // if (connection_str_in_size > 0) hexPrint(log_stream, std::string{static_cast<const char *>(static_cast<const void *>(connection_str_in)), static_cast<size_t>(connection_str_in_size)});
        const auto connection_str = toUTF8(connection_str_in, connection_str_in_size);
        // LOG("connection_str=" << str);
        connection.init(connection_str);
        // Copy complete connection string.
        fillOutputString<SQLTCHAR>(connection.connectionString(), connection_str_out, connection_str_out_max_size, connection_str_out_size, false);
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLPrepare)(HSTMT statement_handle, SQLTCHAR * statement_text, SQLINTEGER statement_text_size) {
    LOG(__FUNCTION__ << " statement_text_size=" << statement_text_size << " statement_text=" << statement_text);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        const auto query = toUTF8(statement_text, statement_text_size);
        statement.prepareQuery(query);
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLExecute)(HSTMT statement_handle) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        statement.executeQuery();
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLExecDirect)(HSTMT statement_handle, SQLTCHAR * statement_text, SQLINTEGER statement_text_size) {
    LOG(__FUNCTION__ << " statement_text_size=" << statement_text_size << " statement_text=" << statement_text);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        const auto query = toUTF8(statement_text, statement_text_size);
        statement.executeQuery(query);
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLNumResultCols)(HSTMT statement_handle, SQLSMALLINT * column_count) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        if (column_count) {
            if (statement.hasResultSet()) {
                *column_count = statement.getNumColumns();
            }
            else {
                *column_count = 0;
            }
        }
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLColAttribute)(
    SQLHSTMT statement_handle,
    SQLUSMALLINT column_number,
    SQLUSMALLINT field_identifier,
    SQLPOINTER out_string_value,
    SQLSMALLINT out_string_value_max_size,
    SQLSMALLINT * out_string_value_size,
#if defined(_win32_) && !defined(_win64_)
    SQLPOINTER out_num_value
#else
    SQLLEN * out_num_value
#endif
) {
    LOG(__FUNCTION__ << "(col=" << column_number << ", field=" << field_identifier << ")");
    auto func = [&](Statement & statement) -> SQLRETURN {
        if (!statement.hasResultSet())
            throw SqlException("Column info is not available", "07009");

        if (column_number < 1 || column_number > statement.getNumColumns())
            throw SqlException("Column number " + std::to_string(column_number) + " is out of range: 1.." +
                std::to_string(statement.getNumColumns()), "07009");

        const auto column_idx = column_number - 1;
        const auto & column_info = statement.getColumnInfo(column_idx);
        const auto & type_info = statement.getTypeInfo(column_info.type, column_info.type_without_parameters);

        switch (field_identifier) {

#define CASE_FIELD_NUM(NAME, VALUE)                                     \
            case NAME:                                                  \
                if (out_num_value)                                      \
                    *reinterpret_cast<SQLLEN *>(out_num_value) = VALUE; \
                return SQL_SUCCESS;

#define CASE_FIELD_STR(NAME, VALUE) \
            case NAME: return fillOutputString<SQLTCHAR>(VALUE, out_string_value, out_string_value_max_size, out_string_value_size, true);

            // TODO: Use IRD (the descriptor) when column data representation is migrated there.

            CASE_FIELD_NUM(SQL_DESC_AUTO_UNIQUE_VALUE, SQL_FALSE);
            CASE_FIELD_STR(SQL_DESC_BASE_COLUMN_NAME, column_info.name);
            CASE_FIELD_STR(SQL_DESC_BASE_TABLE_NAME, "");
            CASE_FIELD_NUM(SQL_DESC_CASE_SENSITIVE, SQL_TRUE);
            CASE_FIELD_STR(SQL_DESC_CATALOG_NAME, "");
            CASE_FIELD_NUM(SQL_DESC_CONCISE_TYPE, type_info.sql_type);

            case SQL_COLUMN_COUNT: /* fallthrough */
            CASE_FIELD_NUM(SQL_DESC_COUNT, statement.getNumColumns());

            CASE_FIELD_NUM(SQL_DESC_DISPLAY_SIZE, column_info.display_size);
            CASE_FIELD_NUM(SQL_DESC_FIXED_PREC_SCALE, SQL_FALSE);
            CASE_FIELD_STR(SQL_DESC_LABEL, column_info.name);

            case SQL_COLUMN_LENGTH: /* fallthrough */ // TODO: alight with ODBCv2 semantics!
            CASE_FIELD_NUM(SQL_DESC_LENGTH, (!type_info.isBufferType() ? 0 :
                std::min<int32_t>(statement.getParent().stringmaxlength, (column_info.fixed_size ? column_info.fixed_size : column_info.display_size))
            ));

            CASE_FIELD_STR(SQL_DESC_LITERAL_PREFIX, "");
            CASE_FIELD_STR(SQL_DESC_LITERAL_SUFFIX, "");
            CASE_FIELD_STR(SQL_DESC_LOCAL_TYPE_NAME, "");

            case SQL_COLUMN_NAME: /* fallthrough */
            CASE_FIELD_STR(SQL_DESC_NAME, column_info.name);

            case SQL_COLUMN_NULLABLE: /* fallthrough */
            CASE_FIELD_NUM(SQL_DESC_NULLABLE, (column_info.is_nullable ? SQL_NULLABLE : SQL_NO_NULLS));

            CASE_FIELD_NUM(SQL_DESC_NUM_PREC_RADIX, (type_info.isIntegerType() ? 10 : 0));
            CASE_FIELD_NUM(SQL_DESC_OCTET_LENGTH, (!type_info.isBufferType() ? type_info.octet_length :
                std::min<int32_t>(statement.getParent().stringmaxlength, (column_info.fixed_size ? column_info.fixed_size : column_info.display_size))
                    * (type_info.isWideCharStringType() ? sizeof(SQLWCHAR) : sizeof(SQLCHAR))
            ));

            case SQL_COLUMN_PRECISION: /* fallthrough */ // TODO: alight with ODBCv2 semantics!
            CASE_FIELD_NUM(SQL_DESC_PRECISION, 0);

            case SQL_COLUMN_SCALE: /* fallthrough */ // TODO: alight with ODBCv2 semantics!
            CASE_FIELD_NUM(SQL_DESC_SCALE, 0);

            CASE_FIELD_STR(SQL_DESC_SCHEMA_NAME, "");
            CASE_FIELD_NUM(SQL_DESC_SEARCHABLE, SQL_SEARCHABLE);
            CASE_FIELD_STR(SQL_DESC_TABLE_NAME, "");
            CASE_FIELD_NUM(SQL_DESC_TYPE, type_info.sql_type);
            CASE_FIELD_STR(SQL_DESC_TYPE_NAME, type_info.sql_type_name);
            CASE_FIELD_NUM(SQL_DESC_UNNAMED, SQL_NAMED);
            CASE_FIELD_NUM(SQL_DESC_UNSIGNED, (type_info.is_unsigned ? SQL_TRUE : SQL_FALSE));
#ifdef SQL_ATTR_READ_ONLY
            CASE_FIELD_NUM(SQL_DESC_UPDATABLE, SQL_ATTR_READ_ONLY);
#else
            CASE_FIELD_NUM(SQL_DESC_UPDATABLE, SQL_FALSE);
#endif

#undef CASE_FIELD_NUM
#undef CASE_FIELD_STR

            default:
                throw SqlException("Driver not capable", "HYC00");
        }
    };

    return CALL_WITH_HANDLE(statement_handle, func);
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLDescribeCol)(HSTMT statement_handle,
    SQLUSMALLINT column_number,
    SQLTCHAR * out_column_name,
    SQLSMALLINT out_column_name_max_size,
    SQLSMALLINT * out_column_name_size,
    SQLSMALLINT * out_type,
    SQLULEN * out_column_size,
    SQLSMALLINT * out_decimal_digits,
    SQLSMALLINT * out_is_nullable) {
    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        if (!statement.hasResultSet())
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
                statement.getParent().stringmaxlength, column_info.fixed_size ? column_info.fixed_size : type_info.column_size);
        if (out_decimal_digits)
            *out_decimal_digits = 0;
        if (out_is_nullable)
            *out_is_nullable = column_info.is_nullable ? SQL_NULLABLE : SQL_NO_NULLS;

        return fillOutputString<SQLTCHAR>(column_info.name, out_column_name, out_column_name_max_size, out_column_name_size, false);
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLFetch)(HSTMT statement_handle) {
    LOG(__FUNCTION__);
    return impl::Fetch(
        statement_handle
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLFetchScroll)(HSTMT statement_handle, SQLSMALLINT orientation, SQLLEN offset) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) -> SQLRETURN {
        if (orientation != SQL_FETCH_NEXT)
            throw SqlException("Fetch type out of range", "HY106");

        return impl::Fetch(statement_handle);
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLGetData)(
    HSTMT statement_handle,
    SQLUSMALLINT column_or_param_number,
    SQLSMALLINT target_type,
    PTR out_value,
    SQLLEN out_value_max_size,
    SQLLEN * out_value_size_or_indicator
) {
    LOG(__FUNCTION__);
    return impl::GetData(
        statement_handle,
        column_or_param_number,
        target_type,
        out_value,
        out_value_max_size,
        out_value_size_or_indicator
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLBindCol)(
    HSTMT statement_handle,
    SQLUSMALLINT column_number,
    SQLSMALLINT target_type,
    PTR out_value,
    SQLLEN out_value_max_size,
    SQLLEN * out_value_size_or_indicator
) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        if (out_value_max_size < 0)
            throw SqlException("Invalid string or buffer length", "HY090");

        if (!statement.hasResultSet())
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
        binding.c_type = target_type;
        binding.value = out_value;
        binding.value_max_size = out_value_max_size;
        binding.value_size = out_value_size_or_indicator;
        binding.indicator = out_value_size_or_indicator;

        statement.bindings[column_number] = binding;

        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLRowCount)(HSTMT statement_handle, SQLLEN * out_row_count) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        if (out_row_count) {
            *out_row_count = statement.getDiagHeader().getAttrAs<SQLLEN>(SQL_DIAG_ROW_COUNT, 0);
            LOG("getNumRows=" << *out_row_count);
        }
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLMoreResults)(HSTMT statement_handle) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) {
        return (statement.advanceToNextResultSet() ? SQL_SUCCESS : SQL_NO_DATA);
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLDisconnect)(HDBC connection_handle) {
    LOG(__FUNCTION__);
    return CALL_WITH_HANDLE(connection_handle, [&](Connection & connection) {
        connection.session->reset();
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetDiagRec)(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLTCHAR * out_sqlstate,
    SQLINTEGER * out_native_error_code,
    SQLTCHAR * out_message,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size
) {
    return impl::GetDiagRec(
        handle_type,
        handle,
        record_number,
        out_sqlstate,
        out_native_error_code,
        out_message,
        out_message_max_size,
        out_message_size
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetDiagField)(
    SQLSMALLINT handle_type,
    SQLHANDLE handle,
    SQLSMALLINT record_number,
    SQLSMALLINT field_id,
    SQLPOINTER out_message,
    SQLSMALLINT out_message_max_size,
    SQLSMALLINT * out_message_size
) {
    return impl::GetDiagField(
        handle_type,
        handle,
        record_number,
        field_id,
        out_message,
        out_message_max_size,
        out_message_size
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLTables)(HSTMT statement_handle,
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
        const auto catalog = toUTF8(catalog_name, catalog_name_length);
        const auto schema = toUTF8(schema_name, schema_name_length);
        const auto table = toUTF8(table_name, table_name_length);
        const auto ttype = toUTF8(table_type, table_type_length);

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
            query << statement.getParent().getDatabase() << "')";
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
                query << " AND TABLE_CAT LIKE '" << catalog << "'";
            //if (schema_name_length)
            //    query << " AND TABLE_SCHEM LIKE '" << schema << "'";
            if (table_name && table_name_length)
                query << " AND TABLE_NAME LIKE '" << table << "'";
            //if (table_type_length)
            //    query << " AND TABLE_TYPE = '" << ttype << "'";

            query << " ORDER BY TABLE_TYPE, TABLE_CAT, TABLE_SCHEM, TABLE_NAME";
        }

        statement.executeQuery(query.str());
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLColumns)(HSTMT statement_handle,
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
        s = toUTF8(catalog_name, catalog_name_length);
        if (s.length() > 0) {
            query << " AND TABLE_CAT LIKE '" << s << "'";
        } else {
            query << " AND TABLE_CAT = currentDatabase()";
        }

        s = toUTF8(schema_name, schema_name_length);
        if (s.length() > 0)
            query << " AND TABLE_SCHEM LIKE '" << s << "'";

        s = toUTF8(table_name, table_name_length);
        if (s.length() > 0)
            query << " AND TABLE_NAME LIKE '" << s << "'";

        s = toUTF8(column_name, column_name_length);
        if (s.length() > 0)
            query << " AND COLUMN_NAME LIKE '" << s << "'";

        query << " ORDER BY TABLE_CAT, TABLE_SCHEM, TABLE_NAME, ORDINAL_POSITION";

        statement.executeQuery(query.str(), IResultMutatorPtr(new ColumnsMutator(&statement.getParent().getParent())));
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetTypeInfo)(
    SQLHSTMT statement_handle,
    SQLSMALLINT type
) {
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

        for (const auto & name_info : types_g) {
            add_query_for_type(name_info.first, name_info.second);
        }

        // TODO (artpaul) check current version of ODBC.
        //
        //      In ODBC 3.x, the SQL date, time, and timestamp data types
        //      are SQL_TYPE_DATE, SQL_TYPE_TIME, and SQL_TYPE_TIMESTAMP, respectively;
        //      in ODBC 2.x, the data types are SQL_DATE, SQL_TIME, and SQL_TIMESTAMP.
        {
            auto info = statement.getParent().getParent().getTypeInfo("Date");
            info.sql_type = SQL_DATE;
            add_query_for_type("Date", info);
        }

        {
            auto info = statement.getParent().getParent().getTypeInfo("DateTime");
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

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLNumParams)(
    SQLHSTMT        StatementHandle,
    SQLSMALLINT *   ParameterCountPtr
) {
    LOG(__FUNCTION__);
    return impl::NumParams(
        StatementHandle,
        ParameterCountPtr
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLNativeSql)(HDBC connection_handle,
    SQLTCHAR * query,
    SQLINTEGER query_length,
    SQLTCHAR * out_query,
    SQLINTEGER out_query_max_length,
    SQLINTEGER * out_query_length) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(connection_handle, [&](Connection & connection) {
        std::string query_str = toUTF8(query, query_length);
        return fillOutputString<SQLTCHAR>(query_str, out_query, out_query_max_length, out_query_length, false);
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLCloseCursor)(HSTMT statement_handle) {
    LOG(__FUNCTION__);

    return CALL_WITH_HANDLE(statement_handle, [&](Statement & statement) -> SQLRETURN {
        statement.closeCursor();
        return SQL_SUCCESS;
    });
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLBrowseConnect)(HDBC connection_handle,
    SQLTCHAR * szConnStrIn,
    SQLSMALLINT cbConnStrIn,
    SQLTCHAR * szConnStrOut,
    SQLSMALLINT cbConnStrOutMax,
    SQLSMALLINT * pcbConnStrOut) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLCancel)(HSTMT StatementHandle) {
    LOG(__FUNCTION__ << "Ignoring SQLCancel " << StatementHandle);
    return SQL_SUCCESS;
    //return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetCursorName)(
    HSTMT StatementHandle, SQLTCHAR * CursorName, SQLSMALLINT BufferLength, SQLSMALLINT * NameLength) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLGetFunctions)(HDBC connection_handle, SQLUSMALLINT FunctionId, SQLUSMALLINT * Supported) {
    LOG(__FUNCTION__ << ":" << __LINE__ << " " << " id=" << FunctionId << " ptr=" << Supported);

#define SET_EXISTS(x) Supported[(x) >> 4] |= (1 << ((x)&0xF))
// #define CLR_EXISTS(x) Supported[(x) >> 4] &= ~(1 << ((x) & 0xF))

    return CALL_WITH_HANDLE(connection_handle, [&](Connection & connection) -> SQLRETURN {
        if (FunctionId == SQL_API_ODBC3_ALL_FUNCTIONS) {
            memset(Supported, 0, sizeof(Supported[0]) * SQL_API_ODBC3_ALL_FUNCTIONS_SIZE);

            SET_EXISTS(SQL_API_SQLALLOCHANDLE);
            SET_EXISTS(SQL_API_SQLBINDCOL);
            SET_EXISTS(SQL_API_SQLBINDPARAMETER);
            //SET_EXISTS(SQL_API_SQLBROWSECONNECT);
            //SET_EXISTS(SQL_API_SQLBULKOPERATIONS);
            //SET_EXISTS(SQL_API_SQLCANCEL);
            //SET_EXISTS(SQL_API_SQLCANCELHANDLE);
            SET_EXISTS(SQL_API_SQLCLOSECURSOR);
            SET_EXISTS(SQL_API_SQLCOLATTRIBUTE);
            //SET_EXISTS(SQL_API_SQLCOLUMNPRIVILEGES);
            SET_EXISTS(SQL_API_SQLCOLUMNS);
            //SET_EXISTS(SQL_API_SQLCOMPLETEASYNC);
            SET_EXISTS(SQL_API_SQLCONNECT);
            SET_EXISTS(SQL_API_SQLCOPYDESC);
            SET_EXISTS(SQL_API_SQLDESCRIBECOL);
            SET_EXISTS(SQL_API_SQLDESCRIBEPARAM);
            SET_EXISTS(SQL_API_SQLDISCONNECT);
            SET_EXISTS(SQL_API_SQLDRIVERCONNECT);
            SET_EXISTS(SQL_API_SQLENDTRAN);
            SET_EXISTS(SQL_API_SQLEXECDIRECT);
            SET_EXISTS(SQL_API_SQLEXECUTE);
            //SET_EXISTS(SQL_API_SQLEXTENDEDFETCH);
            SET_EXISTS(SQL_API_SQLFETCH);
            SET_EXISTS(SQL_API_SQLFETCHSCROLL);
            //SET_EXISTS(SQL_API_SQLFOREIGNKEYS);
            SET_EXISTS(SQL_API_SQLFREEHANDLE);
            SET_EXISTS(SQL_API_SQLFREESTMT);
            SET_EXISTS(SQL_API_SQLGETCONNECTATTR);
            //SET_EXISTS(SQL_API_SQLGETCURSORNAME);
            SET_EXISTS(SQL_API_SQLGETDATA);
            SET_EXISTS(SQL_API_SQLGETDESCFIELD);
            SET_EXISTS(SQL_API_SQLGETDESCREC);
            SET_EXISTS(SQL_API_SQLGETDIAGFIELD);
            SET_EXISTS(SQL_API_SQLGETDIAGREC);
            SET_EXISTS(SQL_API_SQLGETENVATTR);
            SET_EXISTS(SQL_API_SQLGETFUNCTIONS);
            SET_EXISTS(SQL_API_SQLGETINFO);
            SET_EXISTS(SQL_API_SQLGETSTMTATTR);
            SET_EXISTS(SQL_API_SQLGETTYPEINFO);
            SET_EXISTS(SQL_API_SQLMORERESULTS);
            SET_EXISTS(SQL_API_SQLNATIVESQL);
            SET_EXISTS(SQL_API_SQLNUMPARAMS);
            SET_EXISTS(SQL_API_SQLNUMRESULTCOLS);
            //SET_EXISTS(SQL_API_SQLPARAMDATA);
            SET_EXISTS(SQL_API_SQLPREPARE);
            //SET_EXISTS(SQL_API_SQLPRIMARYKEYS);
            //SET_EXISTS(SQL_API_SQLPROCEDURECOLUMNS);
            //SET_EXISTS(SQL_API_SQLPROCEDURES);
            //SET_EXISTS(SQL_API_SQLPUTDATA);
            SET_EXISTS(SQL_API_SQLROWCOUNT);
            SET_EXISTS(SQL_API_SQLSETCONNECTATTR);
            //SET_EXISTS(SQL_API_SQLSETCURSORNAME);
            SET_EXISTS(SQL_API_SQLSETDESCFIELD);
            SET_EXISTS(SQL_API_SQLSETDESCREC);
            SET_EXISTS(SQL_API_SQLSETENVATTR);
            //SET_EXISTS(SQL_API_SQLSETPOS);
            SET_EXISTS(SQL_API_SQLSETSTMTATTR);
            //SET_EXISTS(SQL_API_SQLSPECIALCOLUMNS);
            //SET_EXISTS(SQL_API_SQLSTATISTICS);
            //SET_EXISTS(SQL_API_SQLTABLEPRIVILEGES);
            SET_EXISTS(SQL_API_SQLTABLES);

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

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLParamData)(HSTMT StatementHandle, PTR * Value) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLPutData)(HSTMT StatementHandle, PTR Data, SQLLEN StrLen_or_Ind) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLSetCursorName)(HSTMT StatementHandle, SQLTCHAR * CursorName, SQLSMALLINT NameLength) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLSpecialColumns)(HSTMT StatementHandle,
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

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLStatistics)(HSTMT StatementHandle,
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

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLColumnPrivileges)(HSTMT hstmt,
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

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLDescribeParam)(
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

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLExtendedFetch)(
    SQLHSTMT         StatementHandle,
    SQLUSMALLINT     FetchOrientation,
    SQLLEN           FetchOffset,
    SQLULEN *        RowCountPtr,
    SQLUSMALLINT *   RowStatusArray
) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLForeignKeys)(HSTMT hstmt,
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

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLPrimaryKeys)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szTableName,
    SQLSMALLINT cbTableName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLProcedureColumns)(HSTMT hstmt,
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

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLProcedures)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szProcName,
    SQLSMALLINT cbProcName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLSetPos)(HSTMT hstmt, SQLSETPOSIROW irow, SQLUSMALLINT fOption, SQLUSMALLINT fLock) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLTablePrivileges)(HSTMT hstmt,
    SQLTCHAR * szCatalogName,
    SQLSMALLINT cbCatalogName,
    SQLTCHAR * szSchemaName,
    SQLSMALLINT cbSchemaName,
    SQLTCHAR * szTableName,
    SQLSMALLINT cbTableName) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLBindParameter)(
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

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLBulkOperations)(
    SQLHSTMT         StatementHandle,
    SQLSMALLINT      Operation
) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLCancelHandle)(SQLSMALLINT HandleType, SQLHANDLE Handle) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLCompleteAsync)(SQLSMALLINT HandleType, SQLHANDLE Handle, RETCODE * AsyncRetCodePtr) {
    LOG(__FUNCTION__);
    return SQL_ERROR;
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLEndTran)(
    SQLSMALLINT     HandleType,
    SQLHANDLE       Handle,
    SQLSMALLINT     CompletionType
) {
    LOG(__FUNCTION__);
    return impl::EndTran(
        HandleType,
        Handle,
        CompletionType
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetDescField)(
    SQLHDESC        DescriptorHandle,
    SQLSMALLINT     RecNumber,
    SQLSMALLINT     FieldIdentifier,
    SQLPOINTER      ValuePtr,
    SQLINTEGER      BufferLength,
    SQLINTEGER *    StringLengthPtr
) {
    LOG(__FUNCTION__);
    return impl::GetDescField(
        DescriptorHandle,
        RecNumber,
        FieldIdentifier,
        ValuePtr,
        BufferLength,
        StringLengthPtr
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLGetDescRec)(
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
) {
    LOG(__FUNCTION__);
    return impl::GetDescRec(
        DescriptorHandle,
        RecNumber,
        Name,
        BufferLength,
        StringLengthPtr,
        TypePtr,
        SubTypePtr,
        LengthPtr,
        PrecisionPtr,
        ScalePtr,
        NullablePtr
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION_MAYBE_W(SQLSetDescField)(
    SQLHDESC      DescriptorHandle,
    SQLSMALLINT   RecNumber,
    SQLSMALLINT   FieldIdentifier,
    SQLPOINTER    ValuePtr,
    SQLINTEGER    BufferLength
) {
    LOG(__FUNCTION__);
    return impl::SetDescField(
        DescriptorHandle,
        RecNumber,
        FieldIdentifier,
        ValuePtr,
        BufferLength
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLSetDescRec)(
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
) {
    LOG(__FUNCTION__);
    return impl::SetDescRec(
        DescriptorHandle,
        RecNumber,
        Type,
        SubType,
        Length,
        Precision,
        Scale,
        DataPtr,
        StringLengthPtr,
        IndicatorPtr
    );
}

SQLRETURN SQL_API EXPORTED_FUNCTION(SQLCopyDesc)(
    SQLHDESC     SourceDescHandle,
    SQLHDESC     TargetDescHandle
) {
    LOG(__FUNCTION__);
    return impl::CopyDesc(
        SourceDescHandle,
        TargetDescHandle
    );
}

/*
 *	This function is used to cause the Driver Manager to
 *	call functions by number rather than name, which is faster.
 *	The ordinal value of this function must be 199 to have the
 *	Driver Manager do this.  Also, the ordinal values of the
 *	functions must match the value of fFunction in SQLGetFunctions()
 *
 *	EDIT: not relevant for 3.x drivers. Currently, used for testing dynamic loading only.
 */
SQLRETURN SQL_API EXPORTED_FUNCTION(SQLDummyOrdinal)(void) {
    return SQL_SUCCESS;
}

} // extern "C"
