#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/utils.h"
#include "driver/utils/object_pool.h"
#include "driver/utils/amortized_istream_reader.h"
#include "driver/utils/type_parser.h"
#include "driver/utils/type_info.h"

#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

extern const std::string::size_type initial_string_capacity_g;

class ColumnInfo {
public:
    void assignTypeInfo(const TypeAst & ast);
    void updateTypeInfo();

public:
    std::string name;
    std::string type;
    std::string type_without_parameters;
    DataSourceTypeId type_without_parameters_id = DataSourceTypeId::Unknown;
    std::int64_t display_size = SQL_NO_TOTAL;
    std::size_t display_size_so_far = 0; // Dynamically calculated display size, used for deducing actual display size when the entire result set is processed.
    std::size_t fixed_size = 0;
    std::size_t precision = 0;
    std::size_t scale = 0;
    bool is_nullable = false;
};

class Field {
public:
    using DataType = std::variant<
        DataSourceType< DataSourceTypeId::Date        >,
        DataSourceType< DataSourceTypeId::DateTime    >,
        DataSourceType< DataSourceTypeId::Decimal     >,
        DataSourceType< DataSourceTypeId::Decimal32   >,
        DataSourceType< DataSourceTypeId::Decimal64   >,
        DataSourceType< DataSourceTypeId::Decimal128  >,
        DataSourceType< DataSourceTypeId::FixedString >,
        DataSourceType< DataSourceTypeId::Float32     >,
        DataSourceType< DataSourceTypeId::Float64     >,
        DataSourceType< DataSourceTypeId::Int8        >,
        DataSourceType< DataSourceTypeId::Int16       >,
        DataSourceType< DataSourceTypeId::Int32       >,
        DataSourceType< DataSourceTypeId::Int64       >,
        DataSourceType< DataSourceTypeId::Nothing     >, // ...used for storing Null.
        DataSourceType< DataSourceTypeId::String      >,
        DataSourceType< DataSourceTypeId::UInt8       >,
        DataSourceType< DataSourceTypeId::UInt16      >,
        DataSourceType< DataSourceTypeId::UInt32      >,
        DataSourceType< DataSourceTypeId::UInt64      >,
        DataSourceType< DataSourceTypeId::UUID        >,

        // In case we approach value conversion conservatively...
        WireTypeAnyAsString,
        WireTypeDateAsInt,
        WireTypeDateTimeAsInt
    >;

    template <typename ConversionContext>
    SQLRETURN extract(BindingInfo & binding_info, ConversionContext && context) const;

public:
    DataType data = DataSourceType<DataSourceTypeId::Nothing>{};
};

class Row {
public:
    template <typename ConversionContext>
    SQLRETURN extractField(std::size_t column_idx, BindingInfo & binding_info, ConversionContext && context) const;

public:
    std::vector<Field> fields;
};

class ResultMutator {
public:
    virtual ~ResultMutator() = default;

    virtual void transformRow(const std::vector<ColumnInfo> & columns_info, Row & row) = 0;
};

class ResultSet {
public:
    explicit ResultSet(AmortizedIStreamReader & str, std::unique_ptr<ResultMutator> && mutator);

    virtual ~ResultSet();

    std::unique_ptr<ResultMutator> releaseMutator();

    const ColumnInfo & getColumnInfo(std::size_t column_idx) const;
    std::size_t getColumnCount() const;

    std::size_t fetchRowSet(SQLSMALLINT orientation, SQLLEN offset, std::size_t size);

    std::size_t getCurrentRowSetSize() const;
    std::size_t getCurrentRowSetPosition() const; // 1-based. 1 means the first row of the row set is the first row of the entire result set.
    std::size_t getCurrentRowPosition() const;    // 1-based. 1 means positioned at the first row of the entire result set.
    std::size_t getAffectedRowCount() const;

    // row_idx - row index within the row set.
    SQLRETURN extractField(std::size_t row_idx, std::size_t column_idx, BindingInfo & binding_info);

protected:
    void tryPrefetchRows(std::size_t size);
    void retireRow(Row && row);

    virtual bool readNextRow(Row & row) = 0;

protected:
    AmortizedIStreamReader & stream;
    std::unique_ptr<ResultMutator> result_mutator;
    DefaultConversionContext conversion_context;
    std::vector<ColumnInfo> columns_info;
    std::deque<Row> row_set;
    std::size_t row_set_position = 0; // 1-based. 1 means the first row of the row set is the first row of the entire result set.
    std::size_t row_position = 0;     // 1-based. 1 means positioned at the first row of the entire result set.
    std::size_t affected_row_count = 0;
    std::deque<Row> prefetched_rows;
    bool finished = false;
    ObjectPool<std::string> string_pool;
    ObjectPool<Row> row_pool;
};

class ResultReader {
protected:
    explicit ResultReader(std::istream & stream, std::unique_ptr<ResultMutator> && mutator);

public:
    virtual ~ResultReader() = default;

    bool hasResultSet() const;
    ResultSet & getResultSet();

    std::unique_ptr<ResultMutator> releaseMutator();

    virtual bool advanceToNextResultSet() = 0;

protected:
    AmortizedIStreamReader stream;
    std::unique_ptr<ResultMutator> result_mutator;
    std::unique_ptr<ResultSet> result_set;
};

std::unique_ptr<ResultReader> make_result_reader(const std::string & format, std::istream & raw_stream, std::unique_ptr<ResultMutator> && mutator);

template <typename ConversionContext>
SQLRETURN Field::extract(BindingInfo & binding_info, ConversionContext && context) const {
    return std::visit([&binding_info, &context] (auto & value) {
        if constexpr (std::is_same_v<DataSourceType<DataSourceTypeId::Nothing>, std::decay_t<decltype(value)>>) {
            return fillOutputNULL(binding_info.value, binding_info.value_max_size, binding_info.indicator);
        }
        else {
            return writeDataFrom(value, binding_info, std::forward<ConversionContext>(context));
        }
    }, data);
}

template <typename ConversionContext>
SQLRETURN Row::extractField(std::size_t column_idx, BindingInfo & binding_info, ConversionContext && context) const {
    if (column_idx >= fields.size())
        throw SqlException("Invalid descriptor index", "07009");

    return fields[column_idx].extract(binding_info, std::forward<ConversionContext>(context));
}
