#pragma once

#include "diagnostics.h"

#include <map>
#include <stdexcept>

struct TypeInfo
{
    std::string sql_type_name;
    bool is_unsigned;
    SQLSMALLINT sql_type;
    size_t column_size;
    size_t octet_length;

    inline bool IsIntegerType() const
    {
        return
            sql_type == SQL_TINYINT || sql_type == SQL_SMALLINT ||
            sql_type == SQL_INTEGER || sql_type == SQL_BIGINT;
    }

    inline bool IsStringType() const
    {
        return sql_type == SQL_VARCHAR;
    }
};


struct Environment
{
    Environment();
    ~Environment();

    static const std::map<std::string, TypeInfo> types_info;

    SQLUINTEGER metadata_id = SQL_FALSE;
    int odbc_version = SQL_OV_ODBC3_80;
    DiagnosticRecord diagnostic_record;
};
