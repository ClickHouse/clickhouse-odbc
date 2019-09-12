#include "object.h"
#include "driver.h"

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
