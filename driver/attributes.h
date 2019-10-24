#pragma once

#include "platform.h"
#include "utils.h"

#include <string>
#include <unordered_map>

// TODO: consider upgrading to std::variant of C++17 (or Boost), when available.
class AttributeContainer {
public:
    virtual ~AttributeContainer() = default;

    bool hasAttrInteger(int attr) const;
    bool hasAttrString(int attr) const;
    bool hasAttr(int attr) const;

    template <typename T> inline bool hasAttrAs(int attr) const;

    template <typename T> inline T getAttrAs(int attr, const T & def = T{}) const;

    template <typename T> inline T setAttrSilent(int attr, const T& value);
    template <typename T> inline T setAttr(int attr, const T& value);

    void resetAttr(int attr);
    void resetAttrs();

protected:
    virtual void onAttrChange(int attr);

private:
    std::unordered_map<int, std::int64_t> integers;
    std::unordered_map<int, std::string> strings;
};

template <typename T>
inline bool AttributeContainer::hasAttrAs(int attr) const {
    return hasAttrInteger(attr);
}

template <>
inline bool AttributeContainer::hasAttrAs<std::string>(int attr) const {
    return hasAttrString(attr);
}

template <typename T>
inline T AttributeContainer::getAttrAs(int attr, const T & def) const {
    auto it = integers.find(attr);
    return (it == integers.end() ?
        def : (T)(it->second));
}

template <>
inline std::string AttributeContainer::getAttrAs<std::string>(int attr, const std::string & def) const {
    auto it = strings.find(attr);
    return (it == strings.end() ?
        def : it->second);
}

template <typename T>
inline T AttributeContainer::setAttrSilent(int attr, const T& value) {
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
inline std::string AttributeContainer::setAttrSilent<std::string>(int attr, const std::string& value) {
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
inline T AttributeContainer::setAttr(int attr, const T& value) {
    std::int64_t old_value = 0;
    strings.erase(attr);
    auto it = integers.find(attr);
    if (it == integers.end()) {
        integers.emplace(attr, (std::int64_t)value);
        onAttrChange(attr);
    }
    else {
        old_value = it->second;
        it->second = (std::int64_t)value;
        if (old_value != (std::int64_t)value)
            onAttrChange(attr);
    }
    return (T)old_value;
}

template <>
inline std::string AttributeContainer::setAttr<std::string>(int attr, const std::string& value) {
    std::string old_value;
    integers.erase(attr);
    auto it = strings.find(attr);
    if (it == strings.end()) {
        strings.emplace(attr, value);
        onAttrChange(attr);
    }
    else {
        old_value = std::move(it->second);
        it->second = value;
        if (old_value != value)
            onAttrChange(attr);
    }
    return old_value;
}
