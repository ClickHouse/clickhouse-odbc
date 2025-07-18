#include "driver/object.h"
#include "driver/driver.h"

Object::Object() noexcept
    : handle(this)
{
}

SQLHANDLE Object::getHandle() const noexcept {
    return handle;
}
