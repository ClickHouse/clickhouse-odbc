#include "driver/api/sql_columns_resultset_mutator.h"

// Column position numbers as described in
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function
static const size_t COL_TABLE_CAT = 0;
static const size_t COL_TABLE_SCHEM = 1;
static const size_t COL_TABLE_NAME = 2;
static const size_t COL_COLUMN_NAME = 3;
static const size_t COL_DATA_TYPE = 4;
static const size_t COL_TYPE_NAME = 5;
static const size_t COL_COLUMN_SIZE = 6;
static const size_t COL_BUFFER_LENGTH = 7;
static const size_t COL_DECIMAL_DIGITS = 8;
static const size_t COL_NUM_PREC_RADIX = 9;
static const size_t COL_NULLABLE = 10;
static const size_t COL_REMARKS = 11;
static const size_t COL_COLUMN_DEF = 12;
static const size_t COL_SQL_DATA_TYPE = 13;
static const size_t COL_SQL_DATETIME_SUB = 14;
static const size_t COL_CHAR_OCTET_LENGTH = 15;
static const size_t COL_ORDINAL_POSITION = 16;
static const size_t COL_IS_NULLABLE = 17;

static ColumnInfo parseColumnType(const std::string& type_name)
{
    TypeParser parser{type_name};
    TypeAst ast;
    ColumnInfo ret;
    if (parser.parse(&ast)) {
        ret.assignTypeInfo(ast, Poco::Timezone::name());
        auto type_id = convertUnparametrizedTypeNameToTypeId(ret.type_without_parameters);
        if (type_id == DataSourceTypeId::Unknown) {
            ret.type_without_parameters = "String";
        }
    }
    else {
        ret.type_without_parameters = "String";
    }

    return ret;
}

void SQLColumnsResultSetMutator::transformRow(const std::vector<ColumnInfo> & /*unused*/, Row & row)
{
    using enum DataSourceTypeId;
    // FIXME(slabko): if this get fails, then something is terribly wrong - throw a proper exception
    const auto & type_name_wrapper = std::get<DataSourceType<DataSourceTypeId::String>>(row.fields.at(COL_TYPE_NAME).data);
    const auto & type_name = type_name_wrapper.value;
    const auto column_info = parseColumnType(type_name);
    const TypeInfo & type_info = statement.getTypeInfo(column_info.type, column_info.type_without_parameters);
    row.fields.at(COL_DATA_TYPE).data = DataSourceType<DataSourceTypeId::Int16>{type_info.data_type};
    row.fields.at(COL_TYPE_NAME).data = DataSourceType<DataSourceTypeId::String>{type_info.type_name};

    int column_size{};
    switch (type_info.type_id) {
        case Decimal:
            column_size = column_info.precision;
            break;
        case FixedString:
            column_size = column_info.fixed_size;
            break;
        default:
            column_size = type_info.column_size;
    }

    row.fields.at(COL_COLUMN_SIZE).data = DataSourceType<DataSourceTypeId::Int32>{column_size};

    if (type_info.num_prec_radix.has_value())
        row.fields.at(COL_NUM_PREC_RADIX).data = DataSourceType<DataSourceTypeId::Int16>{*type_info.num_prec_radix};

    row.fields.at(COL_NULLABLE).data = DataSourceType<DataSourceTypeId::Int16>{column_info.is_nullable};

    if (type_info.type_id == Decimal)
        row.fields.at(COL_DECIMAL_DIGITS).data = DataSourceType<DataSourceTypeId::Int16>{column_info.scale};
    if (type_info.type_id == DateTime)
        row.fields.at(COL_DECIMAL_DIGITS).data = DataSourceType<DataSourceTypeId::Int16>{0};
    if (type_info.type_id == DateTime64)
        row.fields.at(COL_DECIMAL_DIGITS).data = DataSourceType<DataSourceTypeId::Int16>{column_info.precision};

    row.fields.at(COL_SQL_DATA_TYPE).data = DataSourceType<DataSourceTypeId::Int16>{type_info.sql_data_type};

    if (type_info.sql_datetime_sub.has_value())
        row.fields.at(COL_SQL_DATETIME_SUB).data = DataSourceType<DataSourceTypeId::Int16>{*type_info.sql_datetime_sub};

    row.fields.at(COL_CHAR_OCTET_LENGTH).data = DataSourceType<DataSourceTypeId::Int32>{type_info.octet_length};

    row.fields.at(COL_IS_NULLABLE).data = DataSourceType<DataSourceTypeId::String>{column_info.is_nullable ? "YES" : "NO"};
}
