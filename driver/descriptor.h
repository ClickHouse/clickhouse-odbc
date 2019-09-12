#pragma once

#include "driver.h"
#include "connection.h"
#include "diagnostics.h"

#include <unordered_map>
#include <variant>

class Descriptor
    : public Child<Connection, Descriptor>
{
private:
    using ChildType = Child<Connection, Descriptor>;

public:
    explicit Descriptor(Connection & connection);
};
