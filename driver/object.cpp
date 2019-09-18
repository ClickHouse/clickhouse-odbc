#include "object.h"
#include "driver.h"

Object::Object() noexcept
    : handle(this)
{
    Driver::getInstance().registerDescendant(*this);
}

Object::Object(SQLHANDLE h) noexcept
    : handle(h)
{
    Driver::getInstance().registerDescendant(*this);
}

Object::~Object() {
    Driver::getInstance().unregisterDescendant(*this);
}

SQLHANDLE Object::getHandle() const noexcept {
    return handle;
}
