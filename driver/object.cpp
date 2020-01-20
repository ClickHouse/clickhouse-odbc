#include "driver/object.h"
#include "driver/driver.h"

Object::Object() noexcept
    : handle(this)
{
}

#if defined(WORKAROUND_ENABLE_SAFE_DISPATCH_ONLY)
Object::Object(SQLHANDLE h) noexcept
    : handle(h)
{
}
#endif

SQLHANDLE Object::getHandle() const noexcept {
    return handle;
}
