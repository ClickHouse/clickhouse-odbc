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

    inline bool isIntegerType() const {
        return sql_type == SQL_TINYINT || sql_type == SQL_SMALLINT || sql_type == SQL_INTEGER || sql_type == SQL_BIGINT;
    }

    inline bool isStringType() const {
        return sql_type == SQL_VARCHAR;
    }
};


class Environment
    : public Child<Driver, Environment>
{
private:
    using ChildType = Child<Driver, Environment>;

public:
    explicit Environment(Driver & driver);

    // Leave unimplemented for general case.
    template <typename T> T& allocateChild();
    template <typename T> void deallocateChild(SQLHANDLE) noexcept;

    static const auto string_max_size = 0xFFFFFF;
    static const std::map<std::string, TypeInfo> types_info;
    const TypeInfo & getTypeInfo(const std::string & type_name, const std::string & type_name_without_parametrs = "") const;

public:
#if defined(SQL_OV_ODBC3_80)
    int odbc_version = SQL_OV_ODBC3_80;
#else
    int odbc_version = SQL_OV_ODBC3;
#endif

private:
    std::unordered_map<SQLHANDLE, std::shared_ptr<Connection>> connections;
};

template <> Connection& Environment::allocateChild<Connection>();
template <> void Environment::deallocateChild<Connection>(SQLHANDLE handle) noexcept;
