#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/read_helpers.h"
#include "driver/utils/type_parser.h"

#include <deque>
#include <memory>
#include <vector>

class Statement;

class Field {
public:
    std::string data;
    bool is_null = false;

    uint64_t getUInt() const;
    int64_t getInt() const;
    float getFloat() const;
    double getDouble() const;

    SQLGUID getGUID() const;

    SQL_NUMERIC_STRUCT getNumeric(const std::int16_t precision, const std::int16_t scale) const;

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
    explicit ResultSet(const std::string & format, std::istream & in_, IResultMutatorPtr && mutator_);

    const ColumnInfo & getColumnInfo(size_t i) const;
    size_t getNumColumns() const;

    bool hasCurrentRow() const;
    const Row & getCurrentRow() const;
    std::size_t getCurrentRowNum() const;
    bool advanceToNextRow();

    IResultMutatorPtr releaseMutator();

private:
    bool endOfSet();
    size_t prepareSomeRows(size_t max_ready_rows = 100);

private:
    std::istream & in;
    IResultMutatorPtr mutator;
    std::vector<ColumnInfo> columns_info;
    std::deque<Row> ready_raw_rows;
    Row current_row;
    std::size_t current_row_num = 0;
    bool finished = false;
};

void assignTypeInfo(const TypeAst & ast, ColumnInfo * info);
