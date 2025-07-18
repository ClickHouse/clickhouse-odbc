#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/utils.h"
#include "driver/attributes.h"
#include "driver/diagnostics.h"

#include <fstream>
#include <memory>

class Object
    : public AttributeContainer
    , public DiagnosticsContainer
{
public:
    Object(const Object &) = delete;
    Object(Object &&) = delete;
    Object& operator= (const Object &) = delete;
    Object& operator= (Object &&) = delete;

    explicit Object() noexcept;

    virtual ~Object() = default;

    SQLHANDLE getHandle() const noexcept;

private:
    SQLHANDLE const handle;
};

class Driver;

template <typename Parent, typename Self>
class Child
    : public Object
    , public std::enable_shared_from_this<Self>
{
public:
    explicit Child(Parent & p) noexcept
        : parent(p)
    {
        // At this point, the object cannot be considered of type `Self` because
        // `Self`'s constructor has not been called yet. This (rightfully) trips
        // UBSan. However, out of all the possible fixes, this is probably the
        // safest.
        getDriver().registerDescendant(*reinterpret_cast<Self *>(this));
    }

    virtual ~Child() {
        // At this point, the object cannot be considered of type `Self` because
        // `Self`'s destructor has already been called. This (rightfully) trips
        // UBSan. However, out of all the possible fixes, this is probably the
        // safest.
        getDriver().unregisterDescendant(*reinterpret_cast<Self *>(this));
    }

    Driver & getDriver() const noexcept {
        return parent.getDriver();
    }

    Parent & getParent() const noexcept {
        return parent;
    }

    const Self & getSelf() const noexcept {
        return *static_cast<const Self *>(this);
    }

    Self & getSelf() noexcept {
        return *static_cast<Self *>(this);
    }

    void deallocateSelf() noexcept {
        parent.template deallocateChild<Self>(getHandle());
    }

    bool isLoggingEnabled() const {
        return parent.isLoggingEnabled();
    }

    std::ostream & getLogStream() {
        return parent.getLogStream();
    }

    void writeLogMessagePrefix(std::ostream & stream) {
        parent.writeLogMessagePrefix(stream);
        stream << "[" << getObjectTypeName<Self>() << "=" << toHexString(getHandle()) << "] ";
    }

private:
    Parent & parent;
};
