#include "driver/result_set.h"
#include "driver/format/ODBCDriver2.h"
#include "driver/format/RowBinaryWithNamesAndTypes.h"

void ColumnInfo::assignTypeInfo(const TypeAst & ast) {
    if (ast.meta == TypeAst::Terminal) {
        type_without_parameters = ast.name;

        switch (convertUnparametrizedTypeNameToTypeId(type_without_parameters)) {
            case DataSourceTypeId::Decimal: {
                if (ast.elements.size() != 2)
                    throw std::runtime_error("Unexpected Decimal type specification syntax");

                precision = ast.elements.front().size;
                scale = ast.elements.back().size;

                break;
            }

            case DataSourceTypeId::Decimal32: {
                if (ast.elements.size() != 1)
                    throw std::runtime_error("Unexpected Decimal32 type specification syntax");

                precision = 9;
                scale = ast.elements.front().size;

                break;
            }

            case DataSourceTypeId::Decimal64: {
                if (ast.elements.size() != 1)
                    throw std::runtime_error("Unexpected Decimal64 type specification syntax");

                precision = 18;
                scale = ast.elements.front().size;

                break;
            }

            case DataSourceTypeId::Decimal128: {
                if (ast.elements.size() != 1)
                    throw std::runtime_error("Unexpected Decimal128 type specification syntax");

                precision = 38;
                scale = ast.elements.front().size;

                break;
            }

            default: {
                if (ast.elements.size() == 1)
                    fixed_size = ast.elements.front().size;

                break;
            }
        }
    }
    else if (ast.meta == TypeAst::Nullable) {
        is_nullable = true;
        assignTypeInfo(ast.elements.front());
    }
    else {
        // Interpret all unsupported types as String.
        type_without_parameters = "String";
    }
}

void ColumnInfo::updateTypeId() {
    type_without_parameters_id = convertUnparametrizedTypeNameToTypeId(type_without_parameters);
}

SQLRETURN Field::extract(BindingInfo & binding_info) const {
    return std::visit([&binding_info] (auto & value) {
        if constexpr (std::is_same_v<DataSourceType<DataSourceTypeId::Nothing>, std::decay_t<decltype(value)>>) {
            return fillOutputNULL(binding_info.value, binding_info.value_max_size, binding_info.indicator);
        }
        else {
            return writeDataFrom(value, binding_info);
        }
    }, data);
}

SQLRETURN Row::extractField(std::size_t column_idx, BindingInfo & binding_info) const {
    if (column_idx >= fields.size())
        throw SqlException("Invalid descriptor index", "07009");

    return fields[column_idx].extract(binding_info);
}

ResultSet::ResultSet(std::istream & stream, std::unique_ptr<ResultMutator> && mutator)
    : raw_stream(stream)
    , result_mutator(std::move(mutator))
    , string_pool(1000000)
    , row_pool(1000000)
{
}

ResultSet::~ResultSet() {
    while (!row_set.empty()) {
        retireRow(std::move(row_set.front()));
        row_set.pop_front();
    }

    while (!prefetched_rows.empty()) {
        retireRow(std::move(prefetched_rows.front()));
        prefetched_rows.pop_front();
    }
}

std::unique_ptr<ResultMutator> ResultSet::releaseMutator() {
    return std::move(result_mutator);
}

const ColumnInfo & ResultSet::getColumnInfo(std::size_t column_idx) const {
    if (column_idx >= columns_info.size())
        throw SqlException("Invalid descriptor index", "07009");

    return columns_info[column_idx];
}

std::size_t ResultSet::fetchRowSet(SQLSMALLINT orientation, SQLLEN offset, std::size_t size) {
    if (orientation != SQL_FETCH_NEXT)
        throw SqlException("Fetch type out of range", "HY106");

    while (!row_set.empty()) {
        retireRow(std::move(row_set.front()));
        row_set.pop_front();
        ++row_set_position;
    }

    if (prefetched_rows.size() < size) {
        constexpr std::size_t prefetch_at_least = 10'000;
        tryPrefetchRows(std::max(size, prefetch_at_least));
    }

    for (std::size_t i = 0; i < size && !prefetched_rows.empty(); ++i) {
        row_set.emplace_back(std::move(prefetched_rows.front()));
        prefetched_rows.pop_front();
        ++affected_row_count;
    }

    if (row_set.empty())
        row_set_position = 0;
    else if (row_set_position == 0)
        row_set_position = 1;

    row_position = row_set_position;

    return row_set.size();
}

std::size_t ResultSet::getColumnCount() const {
    return columns_info.size();
}

std::size_t ResultSet::getCurrentRowSetSize() const {
    return row_set.size();
}

std::size_t ResultSet::getCurrentRowSetPosition() const {
    return row_set_position;
}

std::size_t ResultSet::getCurrentRowPosition() const {
    if (row_position < row_set_position || row_position >= (row_set_position + row_set.size()))
        return 0;

    return row_position;
}

std::size_t ResultSet::getAffectedRowCount() const {
    return affected_row_count;
}

SQLRETURN ResultSet::extractField(std::size_t row_idx, std::size_t column_idx, BindingInfo & binding_info) const {
    if (row_idx >= row_set.size())
        throw SqlException("Invalid cursor position", "HY109");

    return row_set[row_idx].extractField(column_idx, binding_info);
}

void ResultSet::tryPrefetchRows(std::size_t size) {
    while (!finished && prefetched_rows.size() < size) {
        prefetched_rows.emplace_back(row_pool.get());

        auto & row = prefetched_rows.back();
        row.fields.resize(columns_info.size());

        const auto result_set_not_finished = readNextRow(row);

        if (!result_set_not_finished) {
            retireRow(std::move(row));
            prefetched_rows.pop_back();
            finished = true;
            break;
        }

        if (result_mutator)
            result_mutator->updateRow(columns_info, row);
    }
}

void ResultSet::retireRow(Row && row) {
    for (auto & field : row.fields) {
        if (!field.data.valueless_by_exception()) {
            std::visit([this] (auto & value) {
                if constexpr (std::is_same_v<std::string, std::decay_t<decltype(value)>>) {
                    if (value.capacity() > 0)
                        string_pool.put(std::move(value));
                }
                else if constexpr (is_string_data_source_type_v<std::decay_t<decltype(value)>>) {
                    if (value.value.capacity() > 0)
                        string_pool.put(std::move(value.value));
                }
            }, field.data);
        }
    }
    row_pool.put(std::move(row));
}

ResultReader::ResultReader(std::istream & stream, std::unique_ptr<ResultMutator> && mutator)
    : raw_stream(stream)
    , result_mutator(std::move(mutator))
{
}

bool ResultReader::hasResultSet() const {
    return static_cast<bool>(result_set);
}

ResultSet & ResultReader::getResultSet() {
    return *result_set;
}

std::unique_ptr<ResultMutator> ResultReader::releaseMutator() {
    if (result_set)
        result_mutator = result_set->releaseMutator();

    return std::move(result_mutator);
}

std::unique_ptr<ResultReader> make_result_reader(const std::string & format, std::istream & raw_stream, std::unique_ptr<ResultMutator> && mutator) {
    if (format == "ODBCDriver2") {
        return std::make_unique<ODBCDriver2ResultReader>(raw_stream, std::move(mutator));
    }
    else if (format == "RowBinaryWithNamesAndTypes") {
        if (!is_little_endian())
            throw std::runtime_error("'" + format + "' format is supported only on little-endian platforms");

        return std::make_unique<RowBinaryWithNamesAndTypesResultReader>(raw_stream, std::move(mutator));
    }

    throw std::runtime_error("'" + format + "' format is not supported");
}
