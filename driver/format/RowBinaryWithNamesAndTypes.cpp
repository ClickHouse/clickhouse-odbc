#include "driver/format/RowBinaryWithNamesAndTypes.h"

RowBinaryWithNamesAndTypesResultSet::RowBinaryWithNamesAndTypesResultSet(std::istream & stream, std::unique_ptr<ResultMutator> && mutator)
    : ResultSet(stream, std::move(mutator))
{
    std::uint64_t num_columns = 0;
    readSize(num_columns);

    columns_info.resize(num_columns);

    for (std::size_t i = 0; i < num_columns; ++i) {
        readValue(columns_info[i].name);
    }

    for (std::size_t i = 0; i < num_columns; ++i) {
        readValue(columns_info[i].type);

        TypeParser parser{columns_info[i].type};
        TypeAst ast;

        if (parser.parse(&ast)) {
            columns_info[i].assignTypeInfo(ast);
        }
        else {
            // Interpret all unknown types as String.
            columns_info[i].type_without_parameters = "String";
        }

        columns_info[i].updateTypeId();

        // TODO: max_length

    }

    if (result_mutator)
        result_mutator->updateColumnsInfo(columns_info);

    finished = columns_info.empty();

    constexpr std::size_t prefetch_at_least = 10'000;
    tryPrefetchRows(prefetch_at_least);
}

bool RowBinaryWithNamesAndTypesResultSet::readNextRow(Row & row) {
    if (raw_stream.peek() == EOF)
        return false;

    for (std::size_t i = 0; i < row.fields.size(); ++i) {
        readValue(row.fields[i], columns_info[i]);
    }

    return true;
}

void RowBinaryWithNamesAndTypesResultSet::readSize(std::uint64_t & res) {

    // Read an ULEB128 encoded integer from the stream.

    std::uint64_t tmp_res = 0;
    std::uint8_t shift = 0;

    while (true) {
        auto byte = raw_stream.get();

        if (raw_stream.fail() || byte == EOF)
            throw std::runtime_error("Incomplete result received, expected: at least 1 more byte");

        const std::uint64_t chunk = (byte & 0b01111111);
        const std::uint64_t segment = (chunk << shift);

        if (
            (segment >> shift) != chunk ||
            (std::numeric_limits<decltype(shift)>::max() - 7) < shift
        ) {
            throw std::runtime_error("ULEB128 value too big");
        }

        tmp_res |= segment;

        if ((byte & 0b10000000) == 0)
            break;

        shift += 7;
    }

    res = tmp_res;
}

void RowBinaryWithNamesAndTypesResultSet::readValue(bool & res) {
    auto byte = raw_stream.get();

    if (raw_stream.fail() || byte == EOF)
        throw std::runtime_error("Incomplete result received, expected size: 1");

    res = (byte != 0);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(std::string & res) {
    std::uint64_t size = 0;
    readSize(size);

    res.resize(size); // TODO: switch to uninitializing resize().
    raw_stream.read(res.data(), res.size());

    if (raw_stream.gcount() != res.size())
        throw std::runtime_error("Incomplete result received, expected size: " + std::to_string(size));
}

void RowBinaryWithNamesAndTypesResultSet::readValue(Field & field, ColumnInfo & column_info) {
    if (column_info.is_nullable) {
        bool is_null = false;
        readValue(is_null);

        if (is_null) {
            field.data = DataSourceType<DataSourceTypeId::Nothing>{};
            return;
        }
    }

    switch (column_info.type_without_parameters_id) {
        case DataSourceTypeId::Date:        return readValueAs<DataSourceType< DataSourceTypeId::Date        >>(field, column_info);
        case DataSourceTypeId::DateTime:    return readValueAs<DataSourceType< DataSourceTypeId::DateTime    >>(field, column_info);
        case DataSourceTypeId::Decimal:     return readValueAs<DataSourceType< DataSourceTypeId::Decimal     >>(field, column_info);
        case DataSourceTypeId::Decimal32:   return readValueAs<DataSourceType< DataSourceTypeId::Decimal32   >>(field, column_info);
        case DataSourceTypeId::Decimal64:   return readValueAs<DataSourceType< DataSourceTypeId::Decimal64   >>(field, column_info);
        case DataSourceTypeId::Decimal128:  return readValueAs<DataSourceType< DataSourceTypeId::Decimal128  >>(field, column_info);
        case DataSourceTypeId::FixedString: return readValueAs<DataSourceType< DataSourceTypeId::FixedString >>(field, column_info);
        case DataSourceTypeId::Float32:     return readValueAs<DataSourceType< DataSourceTypeId::Float32     >>(field, column_info);
        case DataSourceTypeId::Float64:     return readValueAs<DataSourceType< DataSourceTypeId::Float64     >>(field, column_info);
        case DataSourceTypeId::Int8:        return readValueAs<DataSourceType< DataSourceTypeId::Int8        >>(field, column_info);
        case DataSourceTypeId::Int16:       return readValueAs<DataSourceType< DataSourceTypeId::Int16       >>(field, column_info);
        case DataSourceTypeId::Int32:       return readValueAs<DataSourceType< DataSourceTypeId::Int32       >>(field, column_info);
        case DataSourceTypeId::Int64:       return readValueAs<DataSourceType< DataSourceTypeId::Int64       >>(field, column_info);
        case DataSourceTypeId::Nothing:     return readValueAs<DataSourceType< DataSourceTypeId::Nothing     >>(field, column_info);
        case DataSourceTypeId::String:      return readValueAs<DataSourceType< DataSourceTypeId::String      >>(field, column_info);
        case DataSourceTypeId::UInt8:       return readValueAs<DataSourceType< DataSourceTypeId::UInt8       >>(field, column_info);
        case DataSourceTypeId::UInt16:      return readValueAs<DataSourceType< DataSourceTypeId::UInt16      >>(field, column_info);
        case DataSourceTypeId::UInt32:      return readValueAs<DataSourceType< DataSourceTypeId::UInt32      >>(field, column_info);
        case DataSourceTypeId::UInt64:      return readValueAs<DataSourceType< DataSourceTypeId::UInt64      >>(field, column_info);
        case DataSourceTypeId::UUID:        return readValueAs<DataSourceType< DataSourceTypeId::UUID        >>(field, column_info);
        default:                            throw std::runtime_error("Unable to decode value of type '" + column_info.type + "'");
    }
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Date        > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::DateTime    > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Decimal     > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Decimal32   > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Decimal64   > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Decimal128  > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::FixedString > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Float32     > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Float64     > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Int8        > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Int16       > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Int32       > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Int64       > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::Nothing     > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::String      > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::UInt8       > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::UInt16      > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::UInt32      > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::UInt64      > & dest, ColumnInfo & column_info) {}
void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType< DataSourceTypeId::UUID        > & dest, ColumnInfo & column_info) {}

RowBinaryWithNamesAndTypesResultReader::RowBinaryWithNamesAndTypesResultReader(std::istream & stream, std::unique_ptr<ResultMutator> && mutator)
    : ResultReader(stream, std::move(mutator))
{
    if (stream.peek() == EOF)
        return;

    result_set = std::make_unique<RowBinaryWithNamesAndTypesResultSet>(stream, releaseMutator());
}

bool RowBinaryWithNamesAndTypesResultReader::advanceToNextResultSet() {
    // RowBinaryWithNamesAndTypes format doesn't support multiple result sets in the response,
    // so only a basic cleanup is done here.

    if (result_set) {
        result_mutator = result_set->releaseMutator();
        result_set.reset();
    }

    return hasResultSet();
}
