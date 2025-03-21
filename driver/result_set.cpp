#include "driver/result_set.h"
#include "driver/format/ODBCDriver2.h"
#include "driver/format/RowBinaryWithNamesAndTypes.h"

const std::string::size_type initial_string_capacity_g = std::string{}.capacity();

void ColumnInfo::assignTypeInfo(const TypeAst & ast, const std::string & default_timezone) {
    if (ast.meta == TypeAst::Terminal) {
        type_without_parameters = ast.name;

        switch (convertUnparametrizedTypeNameToTypeId(type_without_parameters)) {
            case DataSourceTypeId::DateTime: {
                if (ast.elements.size() != 0 && ast.elements.size() != 1)
                    throw std::runtime_error("Unexpected DateTime type specification syntax");

                precision = 0;
                timezone = (ast.elements.size() == 1 ? ast.elements.front().name : default_timezone);

                break;
            }

            case DataSourceTypeId::DateTime64: {
                if (ast.elements.size() != 1 && ast.elements.size() != 2)
                    throw std::runtime_error("Unexpected DateTime64 type specification syntax");

                precision = ast.elements.front().size;
                timezone = (ast.elements.size() == 2 ? ast.elements.back().name : default_timezone);

                if (precision < 0 || precision > 9)
                    throw std::runtime_error("Unexpected DateTime64 type specification syntax");

                break;
            }

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
        assignTypeInfo(ast.elements.front(), default_timezone);
    }
    else {
        // Interpret all types with unrecognized ASTs as String.
        type_without_parameters = "String";
    }
}

void ColumnInfo::updateTypeInfo() {
    type_without_parameters_id = convertUnparametrizedTypeNameToTypeId(type_without_parameters);

    switch (type_without_parameters_id) {
        case DataSourceTypeId::FixedString: {
            display_size = fixed_size;
            break;
        }

        case DataSourceTypeId::String: {
            display_size = SQL_NO_TOTAL;
            break;
        }

        default: {
            auto tmp_type_name = convertTypeIdToUnparametrizedCanonicalTypeName(type_without_parameters_id);

            if (
                type_without_parameters_id == DataSourceTypeId::Decimal32 ||
                type_without_parameters_id == DataSourceTypeId::Decimal64 ||
                type_without_parameters_id == DataSourceTypeId::Decimal128
            ) {
                tmp_type_name = "Decimal";
            }

            auto & type_info = typeInfoFor(tmp_type_name);
            display_size = type_info.column_size;
            break;
        }
    }
}

ResultSet::ResultSet(AmortizedIStreamReader & str, std::unique_ptr<ResultMutator> && mutator)
    : stream(str)
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
        constexpr std::size_t prefetch_at_least = 100;
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

SQLRETURN ResultSet::extractField(std::size_t row_idx, std::size_t column_idx, BindingInfo & binding_info) {
    if (row_idx >= row_set.size())
        throw SqlException("Invalid cursor position", "HY109");

    return row_set[row_idx].extractField(column_idx, binding_info, conversion_context);
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

            // Adjust display_size of columns, if not set already, according to display_size_so_far.
            for (std::size_t i = 0; i < columns_info.size(); ++i) {
                auto & column_info = columns_info[i];
                if (column_info.display_size_so_far > 0) {
                    if (column_info.display_size == SQL_NO_TOTAL) {
                        column_info.display_size = column_info.display_size_so_far;
                    }
                    else if (column_info.display_size_so_far > column_info.display_size) {
                        if (
                            column_info.type_without_parameters_id == DataSourceTypeId::String ||
                            column_info.type_without_parameters_id == DataSourceTypeId::FixedString
                        ) {
                            column_info.display_size = column_info.display_size_so_far;
                        }
                    }
                }
            }

            finished = true;
            break;
        }

        if (result_mutator)
            result_mutator->transformRow(columns_info, row);
    }
}

namespace {

    // Using these instead of simple "if constexpr" to workaround VS2017 behavior.

    template <typename T>
    inline void maybe_recycle(ObjectPool<std::string> & string_pool, T & obj,
        std::enable_if_t<
            std::is_same_v<std::string, T>
        >* = 0
    ) {
        if (obj.capacity() > initial_string_capacity_g)
            string_pool.put(std::move(obj));
    }

    template <typename T>
    inline void maybe_recycle(ObjectPool<std::string> & string_pool, T & obj,
        std::enable_if_t<
            is_string_data_source_type_v<T>
        >* = 0
    ) {
        if (obj.value.capacity() > initial_string_capacity_g)
            string_pool.put(std::move(obj.value));
    }

    template <typename T>
    inline void maybe_recycle(ObjectPool<std::string> & string_pool, T & obj,
        std::enable_if_t<
            !std::is_same_v<std::string, T> &&
            !is_string_data_source_type_v<T>
        >* = 0
    ) {
        // Do nothing;
    }

} // namespace

void ResultSet::retireRow(Row && row) {
    for (auto & field : row.fields) {
        if (!field.data.valueless_by_exception()) {
            std::visit([&] (auto & value) {
                maybe_recycle(string_pool, value);
            }, field.data);
        }
    }
    row_pool.put(std::move(row));
}

ResultReader::ResultReader(const std::string & timezone_, std::istream & raw_stream, std::unique_ptr<ResultMutator> && mutator)
    : timezone(timezone_)
    , stream(raw_stream)
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

std::unique_ptr<ResultReader> make_result_reader(const std::string & format, const std::string & timezone, std::istream & raw_stream, std::unique_ptr<ResultMutator> && mutator) {
    if (format == "ODBCDriver2") {
        return std::make_unique<ODBCDriver2ResultReader>(timezone, raw_stream, std::move(mutator));
    }
    else if (format == "RowBinaryWithNamesAndTypes") {
        if (!isLittleEndian())
            throw std::runtime_error("'" + format + "' format is supported only on little-endian platforms");

        return std::make_unique<RowBinaryWithNamesAndTypesResultReader>(timezone, raw_stream, std::move(mutator));
    }

    throw std::runtime_error("'" + format + "' format is not supported");
}
