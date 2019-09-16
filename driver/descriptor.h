#pragma once

#include "driver.h"
#include "connection.h"
#include "diagnostics.h"

#include <unordered_map>
#include <variant>

class DescriptorRecord
    : public AttributeContainer
{
public:
    virtual void on_attr_change(int attr) final override;

    bool has_column_size() const;
    SQLULEN get_column_size() const;

    bool has_decimal_digits() const;
    SQLSMALLINT get_decimal_digits() const;

private:
    void consistency_check();
};

class Descriptor
    : public Child<Connection, Descriptor>
{
private:
    using ChildType = Child<Connection, Descriptor>;

public:
    explicit Descriptor(Connection & connection);

    std::size_t get_record_count() const;
    DescriptorRecord& get_record(std::size_t num, SQLINTEGER current_role);

private:
    std::vector<DescriptorRecord> records;
};
