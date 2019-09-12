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
    virtual ~Object();

    SQLHANDLE get_handle() const noexcept;

private:
    SQLHANDLE const handle;
};

template <typename Parent, typename Self>
class Child
    : public Object
    , public std::enable_shared_from_this<Self>
{
public:
    explicit Child(Parent & p) noexcept
        : parent(p)
    {
    }

    explicit Child(Parent & p, SQLHANDLE h) noexcept
        : Object(h)
        , parent(p)
    {
    }

    Parent & get_parent() const noexcept {
        return parent;
    }

    const Self & get_self() const noexcept {
        return *static_cast<const Self *>(this);
    }

    Self & get_self() noexcept {
        return *static_cast<Self *>(this);
    }

    void deallocate_self() noexcept {
        parent.template deallocate_child<Self>(get_handle());
    }

#if __cplusplus < 201703L
    std::weak_ptr<Self> weak_from_this() noexcept {
        return this->shared_from_this();
    }

    std::weak_ptr<const Self> weak_from_this() const noexcept {
        return this->shared_from_this();
    }
#endif

    bool is_logging_enabled() const {
        return parent.is_logging_enabled();
    }

    std::ostream & get_log_stream() {
        return parent.get_log_stream();
    }

    void write_log_message_prefix(std::ostream & stream) {
        parent.write_log_message_prefix(stream);
        stream << "[" << get_object_type_name<Self>() << "=" << to_hex_string(get_handle()) << "] ";
    }

private:
    Parent & parent;
    std::ofstream log_file_stream;
};
