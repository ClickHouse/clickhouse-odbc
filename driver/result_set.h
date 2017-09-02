#pragma once

#include "read_helpers.h"
#include "platform.h"
#include "type_parser.h"

#include <Poco/NumberParser.h>
#include <Poco/Types.h>

#include <vector>
#include <memory>

class Statement;

class Field
{
public:
    std::string data;

    Poco::UInt64 getUInt() const{ return Poco::NumberParser::parseUnsigned64(data); }
    Poco::Int64 getInt() const  { return Poco::NumberParser::parse64(data); }
    float getFloat() const      { return Poco::NumberParser::parseFloat(data); }
    double getDouble() const    { return Poco::NumberParser::parseFloat(data); }

    SQL_DATE_STRUCT getDate() const;
    SQL_TIMESTAMP_STRUCT getDateTime() const;

private:
    template <typename T>
    void normalizeDate(T& date) const;
};


class Row
{
public:
    Row() {}
    Row(size_t num_columns) : data(num_columns) {}

    std::vector<Field> data;

    bool isValid() const { return !data.empty(); }
};


class Block
{
public:
    using Data = std::vector<Row>;
    Data data;
};


struct ColumnInfo
{
    std::string name;
    std::string type;
    std::string type_without_parameters;
    size_t display_size = 0;
    size_t fixed_size = 0;
    bool is_nullable = false;
};

class IResultMutator
{
public:
    virtual ~IResultMutator() = default;

    virtual void UpdateColumnInfo(std::vector<ColumnInfo> * columns_info) = 0;

    virtual void UpdateRow(const std::vector<ColumnInfo> & columns_info, Row * row) = 0;
};

using IResultMutatorPtr = std::unique_ptr<IResultMutator>;

class ResultSet
{
public:
    void init(Statement * statement_, IResultMutatorPtr mutator_);

    bool empty() const;
    const ColumnInfo & getColumnInfo(size_t i) const;
    size_t getNumColumns() const;
    size_t getNumRows() const;

    Row fetch();

private:
    std::istream & in();

    void throwIncompleteResult() const;

    bool readNextBlock();

private:
    Statement * statement = nullptr;
    IResultMutatorPtr mutator;
    std::vector<ColumnInfo> columns_info;
    Block current_block;
    Block::Data::const_iterator iterator;
    size_t rows = 0;
};

void assignTypeInfo(const TypeAst & ast, ColumnInfo * info);
