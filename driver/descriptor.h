#pragma once

#include "driver/driver.h"
#include "driver/connection.h"
#include "driver/diagnostics.h"

#include <unordered_map>

class DescriptorRecord
    : public AttributeContainer
{
public:
    bool hasColumnSize() const;
    SQLULEN getColumnSize() const;

    bool hasDecimalDigits() const;
    SQLSMALLINT getDecimalDigits() const;

    void consistencyCheck();

protected:
    virtual void onAttrChange(int attr) final override;
};

class Descriptor
    : public Child<Connection, Descriptor>
{
private:
    using ChildType = Child<Connection, Descriptor>;

public:
    explicit Descriptor(Connection & connection);
    Descriptor & operator= (Descriptor & other);

    std::size_t getRecordCount() const;
    DescriptorRecord& getRecord(std::size_t num, SQLINTEGER current_role);

private:
    std::vector<DescriptorRecord> records;
};
