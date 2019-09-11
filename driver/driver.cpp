#include "driver.h"
#include "environment.h"
#include "connection.h"
#include "descriptor.h"
#include "statement.h"

Object::Object() noexcept
    : handle(this)
{
    Driver::get_instance().register_descendant(*this);
}

Object::Object(SQLHANDLE h) noexcept
    : handle(h)
{
    Driver::get_instance().register_descendant(*this);
}

Object::~Object() {
    Driver::get_instance().unregister_descendant(*this);
}

SQLHANDLE Object::get_handle() const noexcept {
    return handle;
}

Driver::Driver() noexcept {
    set_attr_silent(SQL_ATTR_DRIVER_TRACE, SQL_ATTR_DRIVER_TRACE_DEFAULT)
    set_attr_string(SQL_ATTR_DRIVER_TRACEFILE, SQL_ATTR_DRIVER_TRACEFILE_DEFAULT)

    LOG("==================== ODBC Driver instance constructed ====================");
}

Driver::~Driver() {
    LOG("==================== ODBC Driver instance destructing ====================");
}

Driver& Driver::get_instance() noexcept {
    static Driver driver;
    return driver;
}

template <>
Environment& Driver::allocate_child<Environment>() {
    auto child_sptr = std::make_shared<Environment>(*this);
    auto& child = *child_sptr;
    auto handle = child.get_handle();
    environments.emplace(handle, std::move(child_sptr));
    return child;
}

template <>
void Driver::deallocate_child<Environment>(SQLHANDLE handle) noexcept {
    environments.erase(handle);
}

void Driver::register_descendant(Object & descendant) {
    descendants[descendant.get_handle()] = descendant;
}

void Driver::unregister_descendant(Object & descendant) noexcept {
    descendants.erase(descendant.get_handle());
}

template <>
Environment* Driver::dynamic_cast_to<Environment>(Object* obj) {
    return dynamic_cast<Environment*>(obj);
}

template <>
Connection* Driver::dynamic_cast_to<Connection>(Object* obj) {
    return dynamic_cast<Connection*>(obj);
}

template <>
Descriptor* Driver::dynamic_cast_to<Descriptor>(Object* obj) {
    return dynamic_cast<Descriptor*>(obj);
}

template <>
Statement* Driver::dynamic_cast_to<Statement>(Object* obj) {
    return dynamic_cast<Statement*>(obj);
}
