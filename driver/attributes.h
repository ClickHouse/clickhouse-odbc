#pragma once

#include "platform.h"
#include "utils.h"

#include <string>
#include <unordered_map>

#ifdef NDEBUG
#   define DRIVER_TRACE_DEFAULT SQL_OPT_TRACE_OFF
#else
#   define DRIVER_TRACE_DEFAULT SQL_OPT_TRACE_ON
#endif

#ifdef _win_
#   define DRIVER_TRACEFILE_DEFAULT "\\temp\\clickhouse-odbc.log"
#else
#   define DRIVER_TRACEFILE_DEFAULT "/tmp/clickhouse-odbc.log"
#endif

// TODO: consider upgrading to std::variant of C++17 (or Boost), when available.
class AttributeContainer {
public:
    virtual ~AttributeContainer() = default;

    bool has_attr_integer(int attr) const;
    bool has_attr_string(int attr) const;
    bool has_attr(int attr) const;

    template <typename T> inline bool has_attr_as(int attr) const;

    template <typename T> inline T get_attr_as(int attr) const;

    template <typename T> inline void set_attr_silent(int attr, const T& value);
    template <typename T> inline void set_attr(int attr, const T& value);

protected:
    virtual void on_attr_change(int attr);

private:
    std::unordered_map<int, std::int64_t> integers;
    std::unordered_map<int, std::string> strings;
};

template <typename T>
inline bool AttributeContainer::has_attr_as(int attr) const {
    return has_attr_integer(attr);
}

template <>
inline bool AttributeContainer::has_attr_as<std::string>(int attr) const {
    return has_attr_string(attr);
}

template <typename T>
inline T AttributeContainer::get_attr_as(int attr) const {
    auto it = integers.find(attr);
    return (it == integers.end() ?
        T{} : static_cast<T>(it->second));
}

template <>
inline std::string AttributeContainer::get_attr_as<std::string>(int attr) const {
    auto it = strings.find(attr);
    return (it == strings.end() ?
        std::string{} : it->second);
}

template <typename T>
inline void AttributeContainer::set_attr_silent(int attr, const T& value) {
    strings.erase(attr);
    integers[attr] = static_cast<std::int64_t>(value);
}

template <>
inline void AttributeContainer::set_attr_silent<std::string>(int attr, const std::string& value) {
    integers.erase(attr);
    strings[attr] = value;
}

template <typename T>
inline void AttributeContainer::set_attr(int attr, const T& value) {
    set_attr_silent(attr, value);
    on_attr_change(attr);
}

template <>
inline void AttributeContainer::set_attr<std::string>(int attr, const std::string& value) {
    set_attr_silent(attr, value);
    on_attr_change(attr);
}
