#pragma once

#include "platform.h"
#include "utils.h"
#include "attributes.h"
#include "diagnostics.h"

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
    explicit Object(SQLHANDLE h) noexcept;
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
        getDriver().registerDescendant(*this);
    }

    explicit Child(Parent & p, SQLHANDLE h) noexcept
        : Object(h)
        , parent(p)
    {
        getDriver().registerDescendant(*this);
    }

    virtual ~Child() {
        getDriver().unregisterDescendant(*this);
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

#if __cplusplus < 201703L
    std::weak_ptr<Self> weak_from_this() noexcept {
        return this->shared_from_this();
    }

    std::weak_ptr<const Self> weak_from_this() const noexcept {
        return this->shared_from_this();
    }
#endif

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
