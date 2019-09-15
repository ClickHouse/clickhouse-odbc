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

    template <typename T> inline T get_attr_as(int attr, const T & def = T{}) const;

    template <typename T> inline T set_attr_silent(int attr, const T& value);
    template <typename T> inline T set_attr(int attr, const T& value);

    void reset_attrs();

    virtual void on_attr_change(int attr);

private:
    std::unordered_map<int, std::int64_t> integers;
    std::unordered_map<int, std::string> strings;
};

template <typename T>
inline T AttributeContainer::get_attr_as(int attr, const T & def) const {
    auto it = integers.find(attr);
    return (it == integers.end() ?
        def : (T)(it->second));
}

template <>
inline std::string AttributeContainer::get_attr_as<std::string>(int attr, const std::string & def) const {
    auto it = strings.find(attr);
    return (it == strings.end() ?
        def : it->second);
}

template <typename T>
inline T AttributeContainer::set_attr_silent(int attr, const T& value) {
    std::int64_t old_value = 0;
    strings.erase(attr);
    auto it = integers.find(attr);
    if (it == integers.end()) {
        integers.emplace(attr, (std::int64_t)value);
    }
    else {
        old_value = it->second;
        it->second = (std::int64_t)value;
    }
    return (T)old_value;
}

template <>
inline std::string AttributeContainer::set_attr_silent<std::string>(int attr, const std::string& value) {
    std::string old_value;
    integers.erase(attr);
    auto it = strings.find(attr);
    if (it == strings.end()) {
        strings.emplace(attr, value);
    }
    else {
        old_value = std::move(it->second);
        it->second = value;
    }
    return old_value;
}

template <typename T>
inline T AttributeContainer::set_attr(int attr, const T& value) {
    std::int64_t old_value = 0;
    strings.erase(attr);
    auto it = integers.find(attr);
    if (it == integers.end()) {
        integers.emplace(attr, (std::int64_t)value);
        on_attr_change(attr);
    }
    else {
        old_value = it->second;
        it->second = (std::int64_t)value;
        if (old_value != (std::int64_t)value)
            on_attr_change(attr);
    }
    return (T)old_value;
}

template <>
inline std::string AttributeContainer::set_attr<std::string>(int attr, const std::string& value) {
    std::string old_value;
    integers.erase(attr);
    auto it = strings.find(attr);
    if (it == strings.end()) {
        strings.emplace(attr, value);
        on_attr_change(attr);
    }
    else {
        old_value = std::move(it->second);
        it->second = value;
        if (old_value != value)
            on_attr_change(attr);
    }
    return old_value;
}
