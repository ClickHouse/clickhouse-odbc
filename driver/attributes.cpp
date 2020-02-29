#include "driver/attributes.h"

bool AttributeContainer::hasAttrInteger(int attr) const {
    const auto it = attributes.find(attr);
    return (it != attributes.end() && std::holds_alternative<std::intptr_t>(it->second));
}

bool AttributeContainer::hasAttrString(int attr) const {
    const auto it = attributes.find(attr);
    return (it != attributes.end() && std::holds_alternative<std::string>(it->second));
}

bool AttributeContainer::hasAttr(int attr) const {
    const auto it = attributes.find(attr);
    return (it != attributes.end() && !it->second.valueless_by_exception());
}

void AttributeContainer::resetAttr(int attr) {
    attributes.erase(attr);
}

void AttributeContainer::resetAttrs() {
    attributes.clear();
}

void AttributeContainer::onAttrChange(int attr) {
}
