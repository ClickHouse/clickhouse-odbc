#pragma once

#include "driver/driver.h"
#include "driver/diagnostics.h"
#include "driver/utils/type_info.h"

#include <map>
#include <stdexcept>

class Environment
    : public Child<Driver, Environment>
{
private:
    using ChildType = Child<Driver, Environment>;

public:
    explicit Environment(Driver & driver);

    // Leave unimplemented for general case.
    template <typename T> T & allocateChild();
    template <typename T> void deallocateChild(SQLHANDLE) noexcept;

    const TypeInfo & getTypeInfo(const std::string & type_name, const std::string & type_name_without_parameters) const;

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
