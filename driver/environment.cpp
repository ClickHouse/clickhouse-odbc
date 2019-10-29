#include "driver/platform/unicode_t.h"
#include "driver/environment.h"
#include "driver/connection.h"

#if defined(_unix_)
#    include <pwd.h>
#    include <unistd.h>
#endif

#include <sstream>
#include <string>

#include <cstdio>
#include <ctime>

Environment::Environment(Driver & driver)
    : ChildType(driver)
{
}

const TypeInfo & Environment::getTypeInfo(const std::string & type_name, const std::string & type_name_without_parametrs) const {
    auto it = types_g.find(type_name);
    if (it == types_g.end())
        it = types_g.find(type_name_without_parametrs);
    if (it != types_g.end())
        return it->second;
    LOG("Unsupported type " << type_name << " : " << type_name_without_parametrs);
    throw SqlException("Invalid SQL data type", "HY004");
}

template <>
Connection& Environment::allocateChild<Connection>() {
    auto child_sptr = std::make_shared<Connection>(*this);
    auto& child = *child_sptr;
    auto handle = child.getHandle();
    connections.emplace(handle, std::move(child_sptr));
    return child;
}

template <>
void Environment::deallocateChild<Connection>(SQLHANDLE handle) noexcept {
    connections.erase(handle);
}
