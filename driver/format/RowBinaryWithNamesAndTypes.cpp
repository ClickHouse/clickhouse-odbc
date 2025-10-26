#include "driver/format/RowBinaryWithNamesAndTypes.h"
#include "driver/utils/resize_without_initialization.h"

#include <ctime>

RowBinaryWithNamesAndTypesResultSet::RowBinaryWithNamesAndTypesResultSet(const std::string & timezone, AmortizedIStreamReader & stream, std::unique_ptr<ResultMutator> && mutator)
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
            columns_info[i].assignTypeInfo(ast, timezone);
        }
        else {
            throw std::runtime_error("Unable to read values of an unknown type '" + columns_info[i].type + "'");
        }

        columns_info[i].updateTypeInfo();
    }

    finished = columns_info.empty();
}

bool RowBinaryWithNamesAndTypesResultSet::readNextRow(Row & row) {
    if (stream.eof())
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
        const int byte = stream.get();

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

void RowBinaryWithNamesAndTypesResultSet::readValue(bool & dest) {
    const int byte = stream.get();
    dest = (byte != 0);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(std::string & res) {
    std::uint64_t size = 0;
    readSize(size);
    readValue(res, size);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(std::string & dest, const std::uint64_t size) {
    resize_without_initialization(dest, size);

    try {
        stream.read(dest.data(), dest.size());
    }
    catch (...) {
        dest.clear();
        throw;
    }
}

void RowBinaryWithNamesAndTypesResultSet::readValue(Field & dest, ColumnInfo & column_info) {
    if (column_info.is_nullable) {
        bool is_null = false;
        readValue(is_null);

        if (is_null) {
            dest.data = DataSourceType<DataSourceTypeId::Nothing>{};
            return;
        }
    }

    constexpr bool convert_on_fetch_conservatively = true;

    if (convert_on_fetch_conservatively) switch (column_info.type_without_parameters_id) {
        case DataSourceTypeId::Date:        return readValueUsing( WireTypeDateAsInt       (column_info.timezone),                        dest, column_info);
        case DataSourceTypeId::DateTime:    return readValueUsing( WireTypeDateTimeAsInt   (column_info.timezone),                        dest, column_info);
        case DataSourceTypeId::DateTime64:  return readValueUsing( WireTypeDateTime64AsInt (column_info.precision, column_info.timezone), dest, column_info);
        default:                            break; // Continue with the next complete switch...
    }

    switch (column_info.type_without_parameters_id) {
        case DataSourceTypeId::Date:        return readValueAs<DataSourceType< DataSourceTypeId::Date        >>(dest, column_info);
        case DataSourceTypeId::DateTime:    return readValueAs<DataSourceType< DataSourceTypeId::DateTime    >>(dest, column_info);
        case DataSourceTypeId::DateTime64:  return readValueAs<DataSourceType< DataSourceTypeId::DateTime64  >>(dest, column_info);
        case DataSourceTypeId::Decimal:     return readValueAs<DataSourceType< DataSourceTypeId::Decimal     >>(dest, column_info);
        case DataSourceTypeId::Decimal32:   return readValueAs<DataSourceType< DataSourceTypeId::Decimal32   >>(dest, column_info);
        case DataSourceTypeId::Decimal64:   return readValueAs<DataSourceType< DataSourceTypeId::Decimal64   >>(dest, column_info);
        case DataSourceTypeId::Decimal128:  return readValueAs<DataSourceType< DataSourceTypeId::Decimal128  >>(dest, column_info);
        case DataSourceTypeId::FixedString: return readValueAs<DataSourceType< DataSourceTypeId::FixedString >>(dest, column_info);
        case DataSourceTypeId::Float32:     return readValueAs<DataSourceType< DataSourceTypeId::Float32     >>(dest, column_info);
        case DataSourceTypeId::Float64:     return readValueAs<DataSourceType< DataSourceTypeId::Float64     >>(dest, column_info);
        case DataSourceTypeId::Int8:        return readValueAs<DataSourceType< DataSourceTypeId::Int8        >>(dest, column_info);
        case DataSourceTypeId::Int16:       return readValueAs<DataSourceType< DataSourceTypeId::Int16       >>(dest, column_info);
        case DataSourceTypeId::Int32:       return readValueAs<DataSourceType< DataSourceTypeId::Int32       >>(dest, column_info);
        case DataSourceTypeId::Int64:       return readValueAs<DataSourceType< DataSourceTypeId::Int64       >>(dest, column_info);
        case DataSourceTypeId::Nothing:     return readValueAs<DataSourceType< DataSourceTypeId::Nothing     >>(dest, column_info);
        case DataSourceTypeId::String:      return readValueAs<DataSourceType< DataSourceTypeId::String      >>(dest, column_info);
        case DataSourceTypeId::UInt8:       return readValueAs<DataSourceType< DataSourceTypeId::UInt8       >>(dest, column_info);
        case DataSourceTypeId::UInt16:      return readValueAs<DataSourceType< DataSourceTypeId::UInt16      >>(dest, column_info);
        case DataSourceTypeId::UInt32:      return readValueAs<DataSourceType< DataSourceTypeId::UInt32      >>(dest, column_info);
        case DataSourceTypeId::UInt64:      return readValueAs<DataSourceType< DataSourceTypeId::UInt64      >>(dest, column_info);
        case DataSourceTypeId::UUID:        return readValueAs<DataSourceType< DataSourceTypeId::UUID        >>(dest, column_info);
        default:                            throw std::runtime_error("Unable to decode value of type '" + column_info.type + "'");
    }
}

void RowBinaryWithNamesAndTypesResultSet::readValue(WireTypeDateAsInt & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(WireTypeDateTimeAsInt & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(WireTypeDateTime64AsInt & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Date> & dest, ColumnInfo & column_info) {
    WireTypeDateAsInt dest_raw(column_info.timezone);
    readValue(dest_raw, column_info);
    value_manip::from_value<decltype(dest_raw)>::template to_value<decltype(dest)>::convert(dest_raw, dest);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::DateTime> & dest, ColumnInfo & column_info) {
    WireTypeDateTimeAsInt dest_raw(column_info.timezone);
    readValue(dest_raw, column_info);
    value_manip::from_value<decltype(dest_raw)>::template to_value<decltype(dest)>::convert(dest_raw, dest);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::DateTime64> & dest, ColumnInfo & column_info) {
    WireTypeDateTime64AsInt dest_raw(column_info.precision, column_info.timezone);
    readValue(dest_raw, column_info);
    value_manip::from_value<decltype(dest_raw)>::template to_value<decltype(dest)>::convert(dest_raw, dest);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Decimal> & dest, ColumnInfo & column_info) {
    dest.precision = column_info.precision;
    dest.scale = column_info.scale;

    if (dest.precision < 10) {
        std::int32_t value = 0;
        readPOD(value);

        if (value < 0) {
            dest.sign = 0;
            dest.value = -value;
        }
        else {
            dest.sign = 1;
            dest.value = value;
        }
    }
    else if (dest.precision < 19) {
        std::int64_t value = 0;
        readPOD(value);

        if (value < 0) {
            dest.sign = 0;
            dest.value = -value;
        }
        else {
            dest.sign = 1;
            dest.value = value;
        }
    }
    else {
        throw std::runtime_error("Unable to decode value of type 'Decimal' that is represented by 128-bit integer");
    }
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Decimal32> & dest, ColumnInfo & column_info) {
    return readValue(static_cast<DataSourceType<DataSourceTypeId::Decimal> &>(dest), column_info);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Decimal64> & dest, ColumnInfo & column_info) {
    return readValue(static_cast<DataSourceType<DataSourceTypeId::Decimal> &>(dest), column_info);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Decimal128> & dest, ColumnInfo & column_info) {
    return readValue(static_cast<DataSourceType<DataSourceTypeId::Decimal> &>(dest), column_info);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::FixedString> & dest, ColumnInfo & column_info) {
    if (dest.value.capacity() <= initial_string_capacity_g) {
        dest.value = string_pool.get();
        value_manip::to_null(dest.value);
    }

    readValue(dest.value, column_info.fixed_size);

    if (column_info.display_size_so_far < dest.value.size())
        column_info.display_size_so_far = dest.value.size();
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Float32> & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Float64> & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Int8> & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Int16> & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Int32> & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Int64> & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Nothing> & dest, ColumnInfo & column_info) {
    // Do nothing.
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::String> & dest, ColumnInfo & column_info) {
    if (dest.value.capacity() <= initial_string_capacity_g) {
        dest.value = string_pool.get();
        value_manip::to_null(dest.value);
    }

    readValue(dest.value);

    if (column_info.display_size_so_far < dest.value.size())
        column_info.display_size_so_far = dest.value.size();
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::UInt8> & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::UInt16> & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::UInt32> & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::UInt64> & dest, ColumnInfo & column_info) {
    readPOD(dest.value);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::UUID> & dest, ColumnInfo & column_info) {
    char buf[16];

    static_assert(sizeof(dest.value) == lengthof(buf));
    stream.read(buf, lengthof(buf));

    auto * ptr = buf;

    dest.value.Data3 = *reinterpret_cast<decltype(&dest.value.Data3)>(ptr); ptr += sizeof(decltype(dest.value.Data3));
    dest.value.Data2 = *reinterpret_cast<decltype(&dest.value.Data2)>(ptr); ptr += sizeof(decltype(dest.value.Data2));
    dest.value.Data1 = *reinterpret_cast<decltype(&dest.value.Data1)>(ptr); ptr += sizeof(decltype(dest.value.Data1));

    std::copy(ptr, ptr + lengthof(dest.value.Data4), std::make_reverse_iterator(dest.value.Data4 + lengthof(dest.value.Data4)));
}

RowBinaryWithNamesAndTypesResultReader::RowBinaryWithNamesAndTypesResultReader(
    const std::string & timezone_,
    std::istream & raw_stream,
    Poco::Net::HTTPClientSession & session,
    std::unique_ptr<ResultMutator> && mutator
)
    : ResultReader(timezone_, raw_stream, session, std::move(mutator))
{
    if (stream.eof())
        return;

    result_set = std::make_unique<RowBinaryWithNamesAndTypesResultSet>(timezone, stream, releaseMutator());
}

RowBinaryWithNamesAndTypesResultReader::RowBinaryWithNamesAndTypesResultReader(const std::string & timezone_, std::istream & raw_stream, std::unique_ptr<ResultMutator> && mutator, std::unique_ptr<std::istream> && inflating_input_stream)
  : ResultReader(timezone_, raw_stream, std::move(mutator), std::move(inflating_input_stream))
{
    if (stream.eof())
        return;

    result_set = std::make_unique<RowBinaryWithNamesAndTypesResultSet>(timezone, stream, releaseMutator());

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
