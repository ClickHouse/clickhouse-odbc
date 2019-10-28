#include "attributes.h"

bool AttributeContainer::hasAttrInteger(int attr) const {
    auto it = integers.find(attr);
    if (it != integers.end())
        return true;

    return false;
}

bool AttributeContainer::hasAttrString(int attr) const {
    auto it = strings.find(attr);
    if (it != strings.end())
        return true;

    return false;
}

bool AttributeContainer::hasAttr(int attr) const {
    return (hasAttrInteger(attr) || hasAttrString(attr));
}

void AttributeContainer::resetAttr(int attr) {
    integers.erase(attr);
    strings.erase(attr);
}

void AttributeContainer::resetAttrs() {
    integers.clear();
    strings.clear();
}

void AttributeContainer::onAttrChange(int attr) {
}
