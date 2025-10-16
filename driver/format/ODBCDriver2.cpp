#include "driver/format/ODBCDriver2.h"
#include "driver/utils/resize_without_initialization.h"

ODBCDriver2ResultSet::ODBCDriver2ResultSet(
    const std::string & timezone,
    AmortizedIStreamReader & stream,
    std::unique_ptr<ResultMutator> && mutator
)
    : ResultSet(stream, std::move(mutator))
{
    std::int32_t num_header_rows = 0;
    readSize(num_header_rows);

    for (std::size_t row_n = 0; row_n < num_header_rows; ++row_n) {
        std::int32_t num_columns = 0;
        readSize(num_columns);

        if (num_columns <= 1)
            break;

        std::string row_name;
        readValue(row_name);

        --num_columns;

        if (row_name == "name") {
            columns_info.resize(num_columns);

            for (std::size_t i = 0; i < num_columns; ++i) {
                readValue(columns_info[i].name);
            }
        }
        else if (row_name == "type") {
            columns_info.resize(num_columns);

            for (std::size_t i = 0; i < num_columns; ++i) {
                readValue(columns_info[i].type);

                TypeParser parser{columns_info[i].type};
                TypeAst ast;

                if (parser.parse(&ast)) {
                    columns_info[i].assignTypeInfo(ast, timezone);

                    if (convertUnparametrizedTypeNameToTypeId(columns_info[i].type_without_parameters) == DataSourceTypeId::Unknown) {
                        // Interpret all unknown types as String.
                        columns_info[i].type_without_parameters = "String";
                    }
                }
                else {
                    // Interpret all unparsable types as String.
                    columns_info[i].type_without_parameters = "String";
                }

                columns_info[i].updateTypeInfo();
            }
        }
        else {
            for (size_t i = 0; i < num_columns; ++i) {
                std::string dummy_str;
                readValue(dummy_str);
            }
        }
    }

    finished = columns_info.empty();
}

bool ODBCDriver2ResultSet::readNextRow(Row & row) {
    if (stream.eof())
        return false;

    for (std::size_t i = 0; i < row.fields.size(); ++i) {
        readValue(row.fields[i], columns_info[i]);
    }

    return true;
}

void ODBCDriver2ResultSet::readSize(std::int32_t & dest) {
    stream.read(reinterpret_cast<char *>(&dest), sizeof(std::int32_t));
}

void ODBCDriver2ResultSet::readValue(std::string & dest, bool * is_null) {
    std::int32_t size = 0;
    readSize(size);

    if (size >= 0) {
        resize_without_initialization(dest, size);

        if (is_null)
            *is_null = false;

        if (size > 0) {
            try {
                stream.read(dest.data(), size);
            }
            catch (...) {
                dest.clear();
                throw;
            }
        }
    }
    else /*if (size == -1) */{
        dest.clear();

        if (is_null)
            *is_null = true;
    }
}

void ODBCDriver2ResultSet::readValue(Field & dest, ColumnInfo & column_info) {
    auto value = string_pool.get();
    value_manip::to_null(value);

    bool is_null = false;
    readValue(value, &is_null);

    if (is_null/* && column_info.is_nullable*/) {
        dest.data = DataSourceType<DataSourceTypeId::Nothing>{};
        string_pool.put(std::move(value));
        return;
    }

    if (column_info.display_size_so_far < value.size())
        column_info.display_size_so_far = value.size();

    constexpr bool convert_on_fetch_conservatively = true;

    if (convert_on_fetch_conservatively) switch (column_info.type_without_parameters_id) {
        case DataSourceTypeId::FixedString: readValueAs<DataSourceType< DataSourceTypeId::FixedString >>(value, dest, column_info); break;
        case DataSourceTypeId::String:      readValueAs<DataSourceType< DataSourceTypeId::String      >>(value, dest, column_info); break;
        default:                            readValueAs<WireTypeAnyAsString                            >(value, dest, column_info); break;
    }
    else switch (column_info.type_without_parameters_id) {
        case DataSourceTypeId::Date:        readValueAs<DataSourceType< DataSourceTypeId::Date        >>(value, dest, column_info); break;
        case DataSourceTypeId::DateTime:    readValueAs<DataSourceType< DataSourceTypeId::DateTime    >>(value, dest, column_info); break;
        case DataSourceTypeId::DateTime64:  readValueAs<DataSourceType< DataSourceTypeId::DateTime64  >>(value, dest, column_info); break;
        case DataSourceTypeId::Decimal:     readValueAs<DataSourceType< DataSourceTypeId::Decimal     >>(value, dest, column_info); break;
        case DataSourceTypeId::Decimal32:   readValueAs<DataSourceType< DataSourceTypeId::Decimal32   >>(value, dest, column_info); break;
        case DataSourceTypeId::Decimal64:   readValueAs<DataSourceType< DataSourceTypeId::Decimal64   >>(value, dest, column_info); break;
        case DataSourceTypeId::Decimal128:  readValueAs<DataSourceType< DataSourceTypeId::Decimal128  >>(value, dest, column_info); break;
        case DataSourceTypeId::FixedString: readValueAs<DataSourceType< DataSourceTypeId::FixedString >>(value, dest, column_info); break;
        case DataSourceTypeId::Float32:     readValueAs<DataSourceType< DataSourceTypeId::Float32     >>(value, dest, column_info); break;
        case DataSourceTypeId::Float64:     readValueAs<DataSourceType< DataSourceTypeId::Float64     >>(value, dest, column_info); break;
        case DataSourceTypeId::Int8:        readValueAs<DataSourceType< DataSourceTypeId::Int8        >>(value, dest, column_info); break;
        case DataSourceTypeId::Int16:       readValueAs<DataSourceType< DataSourceTypeId::Int16       >>(value, dest, column_info); break;
        case DataSourceTypeId::Int32:       readValueAs<DataSourceType< DataSourceTypeId::Int32       >>(value, dest, column_info); break;
        case DataSourceTypeId::Int64:       readValueAs<DataSourceType< DataSourceTypeId::Int64       >>(value, dest, column_info); break;
        case DataSourceTypeId::Nothing:     readValueAs<DataSourceType< DataSourceTypeId::Nothing     >>(value, dest, column_info); break;
        case DataSourceTypeId::String:      readValueAs<DataSourceType< DataSourceTypeId::String      >>(value, dest, column_info); break;
        case DataSourceTypeId::UInt8:       readValueAs<DataSourceType< DataSourceTypeId::UInt8       >>(value, dest, column_info); break;
        case DataSourceTypeId::UInt16:      readValueAs<DataSourceType< DataSourceTypeId::UInt16      >>(value, dest, column_info); break;
        case DataSourceTypeId::UInt32:      readValueAs<DataSourceType< DataSourceTypeId::UInt32      >>(value, dest, column_info); break;
        case DataSourceTypeId::UInt64:      readValueAs<DataSourceType< DataSourceTypeId::UInt64      >>(value, dest, column_info); break;
        case DataSourceTypeId::UUID:        readValueAs<DataSourceType< DataSourceTypeId::UUID        >>(value, dest, column_info); break;
        default:                            throw std::runtime_error("Unable to decode value of type '" + column_info.type + "'");
    }

    if (value.capacity() > initial_string_capacity_g)
        string_pool.put(std::move(value));
}

void ODBCDriver2ResultSet::readValue(std::string & src, WireTypeAnyAsString & dest, ColumnInfo & column_info) {
    dest.value = std::move(src);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Date> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Date>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::DateTime> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::DateTime>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::DateTime64> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::DateTime64>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Decimal> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Decimal>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Decimal32> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Decimal32>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Decimal64> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Decimal64>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Decimal128> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Decimal128>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::FixedString> & dest, ColumnInfo & column_info) {
    dest.value = std::move(src);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Float32> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Float32>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Float64> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Float64>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Int8> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Int8>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Int16> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Int16>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Int32> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Int32>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Int64> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::Int64>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::Nothing> & dest, ColumnInfo & column_info) {
    // Do nothing.
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::String> & dest, ColumnInfo & column_info) {
    dest.value = std::move(src);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::UInt8> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::UInt8>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::UInt16> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::UInt16>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::UInt32> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::UInt32>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::UInt64> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::UInt64>>::convert(src, dest);
}

void ODBCDriver2ResultSet::readValue(std::string & src, DataSourceType<DataSourceTypeId::UUID> & dest, ColumnInfo & column_info) {
    return value_manip::from_value<std::string>::template to_value<DataSourceType<DataSourceTypeId::UUID>>::convert(src, dest);
}

ODBCDriver2ResultReader::ODBCDriver2ResultReader(
    const std::string & timezone_,
    std::istream & raw_stream,
    Poco::Net::HTTPClientSession & session,
    std::unique_ptr<ResultMutator> && mutator
)
    : ResultReader(timezone_, raw_stream, session, std::move(mutator))
{
    if (stream.eof())
        return;

    result_set = std::make_unique<ODBCDriver2ResultSet>(timezone, stream, releaseMutator());
}

bool ODBCDriver2ResultReader::advanceToNextResultSet() {
    // ODBCDriver2 format doesn't support multiple result sets in the response,
    // so only a basic cleanup is done here.

    if (result_set) {
        result_mutator = result_set->releaseMutator();
        result_set.reset();
    }

    return hasResultSet();
}
