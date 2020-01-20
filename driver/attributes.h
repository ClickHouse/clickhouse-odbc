#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/utils.h"

#include <string>
#include <unordered_map>
#include <variant>

class AttributeContainer {
public:
    virtual ~AttributeContainer() = default;

    bool hasAttrInteger(int attr) const;
    bool hasAttrString(int attr) const;
    bool hasAttr(int attr) const;

    template <typename T> inline bool hasAttrAs(int attr) const;

    template <typename T> inline T getAttrAs(int attr, const T & def = T{}) const;

    template <typename T> inline void setAttrSilent(int attr, const T& value);
    template <typename T> inline void setAttr(int attr, const T& value);

    void resetAttr(int attr);
    void resetAttrs();

protected:
    virtual void onAttrChange(int attr);

private:
    std::unordered_map<int, std::variant<std::intptr_t, std::string>> attributes;
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
    const auto it = attributes.find(attr);

    if (it == attributes.end() || it->second.valueless_by_exception())
        return def;

    return std::visit([] (auto & value) -> T {
        using ValueType = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<ValueType, std::string>) {
            if constexpr (std::is_same_v<T, std::string>)
                return value;
            else {
                const auto num_value = fromString<std::intptr_t>(value);
                if constexpr (std::is_pointer_v<T>)
                    return reinterpret_cast<T>(num_value);
                else
                    return static_cast<T>(num_value);
            }
        }
        else {
            if constexpr (std::is_same_v<T, std::string>)
                return std::to_string(value);
            else if constexpr (std::is_pointer_v<T>)
                return reinterpret_cast<T>(value);
            else
                return static_cast<T>(value);
        }
    }, it->second);
}

template <typename T>
inline void AttributeContainer::setAttrSilent(int attr, const T& value) {
    if constexpr (std::is_pointer_v<T>)
        attributes.insert_or_assign(attr, reinterpret_cast<std::intptr_t>(value));
    else
        attributes.insert_or_assign(attr, value);
}

template <typename T>
inline void AttributeContainer::setAttr(int attr, const T& value) {
    const auto it = attributes.find(attr);

    if (it == attributes.end()) {
        if constexpr (std::is_pointer_v<T>)
            attributes.emplace(attr, reinterpret_cast<std::intptr_t>(value));
        else
            attributes.emplace(attr, value);

        onAttrChange(attr);
    }
    else {
        const bool changed = std::visit(
            [&new_value = value] (auto & old_value) {
                using NewValueType = std::decay_t<decltype(new_value)>;
                using OldValueType = std::decay_t<decltype(old_value)>;

                if constexpr (std::is_same_v<OldValueType, std::string>) {
                    if constexpr (std::is_same_v<NewValueType, std::string>)
                        return (old_value != new_value);
                }
                else {
                    if constexpr (std::is_pointer_v<NewValueType>)
                        return (old_value != reinterpret_cast<std::intptr_t>(new_value));
                    else if constexpr (!std::is_same_v<NewValueType, std::string>)
                        return (old_value != static_cast<std::intptr_t>(new_value));
                }

                return true;
            },
            it->second
        );

        if (changed) {
            if constexpr (std::is_pointer_v<T>)
                it->second = reinterpret_cast<std::intptr_t>(value);
            else
                it->second = value;

            onAttrChange(attr);
        }
    }
}
