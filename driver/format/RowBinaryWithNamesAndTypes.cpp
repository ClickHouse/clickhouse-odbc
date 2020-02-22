#include "driver/format/RowBinaryWithNamesAndTypes.h"

#include <ctime>

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

        columns_info[i].updateTypeInfo();
    }

    finished = columns_info.empty();
}

bool RowBinaryWithNamesAndTypesResultSet::readNextRow(Row & row) {
    if (raw_stream.peek() == EOF) {
        // Adjust display_size of columns, is not set already, according to display_size_so_far.
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

        return false;
    }

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

void RowBinaryWithNamesAndTypesResultSet::readValue(bool & dest) {
    auto byte = raw_stream.get();

    if (raw_stream.fail() || byte == EOF)
        throw std::runtime_error("Incomplete result received, expected size: 1");

    dest = (byte != 0);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(std::string & res) {
    std::uint64_t size = 0;
    readSize(size);
    readValue(res, size);
}

void RowBinaryWithNamesAndTypesResultSet::readValue(std::string & dest, const std::uint64_t size) {
    dest.resize(size); // TODO: switch to uninitializing resize().
    raw_stream.read(dest.data(), dest.size());

    if (raw_stream.gcount() != dest.size())
        throw std::runtime_error("Incomplete result received, expected size: " + std::to_string(size));
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

    switch (column_info.type_without_parameters_id) {
        case DataSourceTypeId::Date:        return readValueAs<DataSourceType< DataSourceTypeId::Date        >>(dest, column_info);
        case DataSourceTypeId::DateTime:    return readValueAs<DataSourceType< DataSourceTypeId::DateTime    >>(dest, column_info);
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

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::Date> & dest, ColumnInfo & column_info) {
    std::uint16_t days_since_epoch = 0;
    readPOD(days_since_epoch);

    std::time_t time = days_since_epoch;
    time = time * 24 * 60 * 60; // Now it's seconds since epoch.
    const auto & tm = *std::localtime(&time);

    dest.value.year = 1900 + tm.tm_year;
    dest.value.month = 1 + tm.tm_mon;
    dest.value.day = tm.tm_mday;
}

void RowBinaryWithNamesAndTypesResultSet::readValue(DataSourceType<DataSourceTypeId::DateTime> & dest, ColumnInfo & column_info) {
    std::uint32_t secs_since_epoch = 0;
    readPOD(secs_since_epoch);

    std::time_t time = secs_since_epoch;
    const auto & tm = *std::localtime(&time);

    dest.value.year = 1900 + tm.tm_year;
    dest.value.month = 1 + tm.tm_mon;
    dest.value.day = tm.tm_mday;
    dest.value.hour = tm.tm_hour;
    dest.value.minute = tm.tm_min;
    dest.value.second = tm.tm_sec;
    dest.value.fraction = 0;
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

    raw_stream.read(buf, lengthof(buf));

    if (raw_stream.gcount() != lengthof(buf))
        throw std::runtime_error("Incomplete result received, expected size: " + std::to_string(lengthof(buf)));

    auto * ptr = buf;

    dest.value.Data3 = *reinterpret_cast<decltype(&dest.value.Data3)>(ptr); ptr += sizeof(decltype(dest.value.Data3));
    dest.value.Data2 = *reinterpret_cast<decltype(&dest.value.Data2)>(ptr); ptr += sizeof(decltype(dest.value.Data2));
    dest.value.Data1 = *reinterpret_cast<decltype(&dest.value.Data1)>(ptr); ptr += sizeof(decltype(dest.value.Data1));

    std::copy(ptr, ptr + lengthof(dest.value.Data4), std::make_reverse_iterator(dest.value.Data4 + lengthof(dest.value.Data4)));
}

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
