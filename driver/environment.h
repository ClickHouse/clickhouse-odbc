#pragma once

#include "driver.h"
#include "diagnostics.h"

#include <map>
#include <stdexcept>

struct TypeInfo {
    std::string sql_type_name;
    bool is_unsigned;
    SQLSMALLINT sql_type;
    int32_t column_size;
    int32_t octet_length;

    inline bool IsIntegerType() const {
        return sql_type == SQL_TINYINT || sql_type == SQL_SMALLINT || sql_type == SQL_INTEGER || sql_type == SQL_BIGINT;
    }

    inline bool IsStringType() const {
        return sql_type == SQL_VARCHAR;
    }
};


class Environment
    : public Child<Driver, Environment>
{
public:
    Environment();

    // Leave unimplemented for general case.
    template <typename T> T& allocate_child();
    template <typename T> void deallocate_child(SQLHANDLE) noexcept;

    template <> Connection& allocate_child<Connection>();
    template <> void deallocate_child<Connection>(SQLHANDLE handle) noexcept;

    static const auto string_max_size = 0xFFFFFF;
    static const std::map<std::string, TypeInfo> types_info;
    const TypeInfo & getTypeInfo(const std::string & type_name, const std::string & type_name_without_parametrs = "") const;

public:
    SQLUINTEGER metadata_id = SQL_FALSE;
    int odbc_version =
#if defined(SQL_OV_ODBC3_80)
        SQL_OV_ODBC3_80;
#else
        SQL_OV_ODBC3;
#endif

    DiagnosticRecord diagnostic_record;

private:
    std::unordered_map<SQLHANDLE, std::shared_ptr<Connection>> connections;
};
