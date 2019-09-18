#include "object.h"
#include "driver.h"

Object::Object() noexcept
    : handle(this)
{
}

Object::Object(SQLHANDLE h) noexcept
    : handle(h)
{
}

SQLHANDLE Object::getHandle() const noexcept {
    return handle;
}
