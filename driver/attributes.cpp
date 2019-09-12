#include "attributes.h"

bool AttributeContainer::has_attr_integer(int attr) const {
    auto iit = integers.find(attr);
    if (iit != integers.end())
        return true;

    return false;
}

bool AttributeContainer::has_attr_string(int attr) const {
    auto sit = strings.find(attr);
    if (sit != strings.end())
        return true;

    return false;
}

bool AttributeContainer::has_attr(int attr) const {
    return (has_attr_integer(attr) || has_attr_string(attr));
}

void AttributeContainer::on_attr_change(int attr) {
}
