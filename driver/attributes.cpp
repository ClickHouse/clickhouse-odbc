#include "attributes.h"

bool AttributeContainer::has_attr_integer(int attr) const {
    auto it = integers.find(attr);
    if (it != integers.end())
        return true;

    return false;
}

bool AttributeContainer::has_attr_string(int attr) const {
    auto it = strings.find(attr);
    if (it != strings.end())
        return true;

    return false;
}

bool AttributeContainer::has_attr(int attr) const {
    return (has_attr_integer(attr) || has_attr_string(attr));
}

void AttributeContainer::reset_attrs() {
    integers.clear();
    strings.clear();
}

void AttributeContainer::on_attr_change(int attr) {
}
