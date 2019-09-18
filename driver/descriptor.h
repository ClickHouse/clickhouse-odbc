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
    bool hasColumnSize() const;
    SQLULEN getColumnSize() const;

    bool hasDecimalDigits() const;
    SQLSMALLINT getDecimalDigits() const;

protected:
    virtual void onAttrChange(int attr) final override;

private:
    void consistencyCheck();
};

class Descriptor
    : public Child<Connection, Descriptor>
{
private:
    using ChildType = Child<Connection, Descriptor>;

public:
    explicit Descriptor(Connection & connection);

    std::size_t getRecordCount() const;
    DescriptorRecord& getRecord(std::size_t num, SQLINTEGER current_role);

private:
    std::vector<DescriptorRecord> records;
};
