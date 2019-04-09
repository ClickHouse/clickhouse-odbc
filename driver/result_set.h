#pragma once

#include <deque>
#include <memory>
#include <vector>
#include "platform.h"
#include "read_helpers.h"
#include "type_parser.h"

class Statement;

class Field {
public:
    std::string data;
    bool is_null = false;

    uint64_t getUInt() const;
    int64_t getInt() const;
    float getFloat() const;
    double getDouble() const;

    SQL_DATE_STRUCT getDate() const;
    SQL_TIMESTAMP_STRUCT getDateTime() const;

private:
    template <typename T>
    void normalizeDate(T & date) const;
};


class Row {
public:
    Row() {}
    Row(size_t num_columns) : data(num_columns) {}

    std::vector<Field> data;

    bool isValid() const {
        return !data.empty();
    }
};


class Block {
public:
    using Data = std::vector<Row>;
    Data data;
};


struct ColumnInfo {
    std::string name;
    std::string type;
    std::string type_without_parameters;
    size_t display_size = 0;
    size_t fixed_size = 0;
    bool is_nullable = false;
};

class IResultMutator {
public:
    virtual ~IResultMutator() = default;

    virtual void UpdateColumnInfo(std::vector<ColumnInfo> * columns_info) = 0;

    virtual void UpdateRow(const std::vector<ColumnInfo> & columns_info, Row * row) = 0;
};

using IResultMutatorPtr = std::unique_ptr<IResultMutator>;

class ResultSet {
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
    bool readNextBlockCache();

private:
    Statement * statement = nullptr;
    IResultMutatorPtr mutator;
    std::vector<ColumnInfo> columns_info;
    std::deque<Row> current_block_buffer;
    Block current_block;
    Block::Data::const_iterator iterator;
    size_t rows = 0;
};

void assignTypeInfo(const TypeAst & ast, ColumnInfo * info);
