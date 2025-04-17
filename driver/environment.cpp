#include "driver/environment.h"
#include "driver/connection.h"

#include <string>

Environment::Environment(Driver & driver)
    : ChildType(driver)
{
}

const TypeInfo & Environment::getTypeInfo(const std::string & type_name, const std::string & type_name_without_parameters) const {
    const TypeInfo * type_info = typeInfoIfExistsFor(type_name);

    if (!type_info)
        type_info = typeInfoIfExistsFor(type_name_without_parameters);

    if (!type_info) {
        const auto tmp_type_without_parameters_id = convertUnparametrizedTypeNameToTypeId(type_name_without_parameters);
        auto tmp_type_name = convertTypeIdToUnparametrizedCanonicalTypeName(tmp_type_without_parameters_id);

        if (
            tmp_type_without_parameters_id == DataSourceTypeId::Decimal32 ||
            tmp_type_without_parameters_id == DataSourceTypeId::Decimal64 ||
            tmp_type_without_parameters_id == DataSourceTypeId::Decimal128
        ) {
            tmp_type_name = "Decimal";
        }

        type_info = typeInfoIfExistsFor(tmp_type_name);
    }

    if (type_info)
        return *type_info;

    LOG("Unsupported type " << type_name << " : " << type_name_without_parameters);

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
