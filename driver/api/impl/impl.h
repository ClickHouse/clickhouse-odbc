#include "driver/platform/platform.h"

namespace impl {

    SQLRETURN allocEnv(
        SQLHENV * out_environment_handle
    ) noexcept;

    SQLRETURN allocConnect(
        SQLHENV environment_handle,
        SQLHDBC * out_connection_handle
    ) noexcept;

    SQLRETURN allocStmt(
        SQLHDBC connection_handle,
        SQLHSTMT * out_statement_handle
    ) noexcept;

    SQLRETURN allocDesc(
        SQLHDBC connection_handle,
        SQLHDESC * out_descriptor_handle
    ) noexcept;
    
    SQLRETURN freeHandle(
        SQLHANDLE handle
    ) noexcept;

    SQLRETURN SetEnvAttr(
        SQLHENV environment_handle,
        SQLINTEGER attribute,
        SQLPOINTER value,
        SQLINTEGER value_length
    ) noexcept;

    SQLRETURN GetEnvAttr(
        SQLHENV environment_handle,
        SQLINTEGER attribute,
        SQLPOINTER out_value,
        SQLINTEGER out_value_max_length,
        SQLINTEGER * out_value_length
    ) noexcept;

    SQLRETURN SetConnectAttr(
        SQLHDBC connection_handle,
        SQLINTEGER attribute,
        SQLPOINTER value,
        SQLINTEGER value_length
    ) noexcept;

    SQLRETURN GetConnectAttr(
        SQLHDBC connection_handle,
        SQLINTEGER attribute,
        SQLPOINTER out_value,
        SQLINTEGER out_value_max_length,
        SQLINTEGER * out_value_length
    ) noexcept;

    SQLRETURN SetStmtAttr(
        SQLHSTMT statement_handle,
        SQLINTEGER attribute,
        SQLPOINTER value,
        SQLINTEGER value_length
    ) noexcept;

    SQLRETURN GetStmtAttr(
        SQLHSTMT statement_handle,
        SQLINTEGER attribute,
        SQLPOINTER out_value,
        SQLINTEGER out_value_max_length,
        SQLINTEGER * out_value_length
    ) noexcept;

    SQLRETURN GetDiagRec(
        SQLSMALLINT handle_type,
        SQLHANDLE handle,
        SQLSMALLINT record_number,
        SQLTCHAR * out_sqlstate,
        SQLINTEGER * out_native_error_code,
        SQLTCHAR * out_message,
        SQLSMALLINT out_message_max_size,
        SQLSMALLINT * out_message_size
    ) noexcept;

    SQLRETURN GetDiagField(
        SQLSMALLINT handle_type,
        SQLHANDLE handle,
        SQLSMALLINT record_number,
        SQLSMALLINT field_id,
        SQLPOINTER out_message,
        SQLSMALLINT out_message_max_size,
        SQLSMALLINT * out_message_size
    ) noexcept;

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
    ) noexcept;

    SQLRETURN NumParams(
        SQLHSTMT        handle,
        SQLSMALLINT *   out_parameter_count
    ) noexcept;

    SQLRETURN DescribeParam(
        SQLHSTMT        handle,
        SQLUSMALLINT    parameter_number,
        SQLSMALLINT *   out_data_type_ptr,
        SQLULEN *       out_parameter_size_ptr,
        SQLSMALLINT *   out_decimal_digits_ptr,
        SQLSMALLINT *   out_nullable_ptr
    ) noexcept;

    SQLRETURN GetDescField(
        SQLHDESC        DescriptorHandle,
        SQLSMALLINT     RecNumber,
        SQLSMALLINT     FieldIdentifier,
        SQLPOINTER      ValuePtr,
        SQLINTEGER      BufferLength,
        SQLINTEGER *    StringLengthPtr
    ) noexcept;

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
    ) noexcept;

    SQLRETURN SetDescField(
        SQLHDESC      DescriptorHandle,
        SQLSMALLINT   RecNumber,
        SQLSMALLINT   FieldIdentifier,
        SQLPOINTER    ValuePtr,
        SQLINTEGER    BufferLength
    ) noexcept;

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
    ) noexcept;

    SQLRETURN CopyDesc(
        SQLHDESC     SourceDescHandle,
        SQLHDESC     TargetDescHandle
    ) noexcept;

    SQLRETURN EndTran(
        SQLSMALLINT     handle_type,
        SQLHANDLE       handle,
        SQLSMALLINT     completion_type
    ) noexcept;

    SQLRETURN GetData(
        HSTMT statement_handle,
        SQLUSMALLINT column_or_param_number,
        SQLSMALLINT target_type,
        PTR out_value,
        SQLLEN out_value_max_size,
        SQLLEN * out_value_size_or_indicator
    ) noexcept;

    SQLRETURN Fetch(
        HSTMT statement_handle
    ) noexcept;

} // namespace impl
