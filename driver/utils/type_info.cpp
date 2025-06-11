#include "driver/utils/type_info.h"

#include <Poco/String.h>
#include <stdexcept>
#include <unordered_map>

const TypeInfo* typeInfoIfExistsFor(const std::string & type) {
    static const auto types = [](){
        std::unordered_map<std::string, const TypeInfo> ret{};
        ret.reserve(TypeInfoCatalog::Types.size());
        for (const auto& type_info : TypeInfoCatalog::Types)
            ret.insert({type_info.type_name, type_info});

        return ret;
    }();

    const auto it = types.find(type);
    if (it == types.end())
        return nullptr;

    return &it->second;
}

const TypeInfo & typeInfoFor(const std::string & type) {
    if (auto type_info = typeInfoIfExistsFor(type)) {
        return *type_info;
    }
    throw std::runtime_error("unknown type");
}

DataSourceTypeId convertUnparametrizedTypeNameToTypeId(const std::string & type_name) {
    if (auto type_info = typeInfoIfExistsFor(type_name)) {
        return type_info->type_id;
    }
    return DataSourceTypeId::Unknown;
}

std::string convertTypeIdToUnparametrizedCanonicalTypeName(DataSourceTypeId type_id) {
    const auto& idx = DataSourceTypeIdIndex(type_id);

    if (idx < 0 || idx >= TypeInfoCatalog::Types.size()) {
        throw std::out_of_range("unknown type id");
    }

    const auto& type = TypeInfoCatalog::Types[idx];

    return std::string(type.type_name);
}

SQLSMALLINT convertSQLTypeToCType(SQLSMALLINT sql_type) noexcept {
    switch (sql_type) {
        case SQL_TYPE_NULL:
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
            return SQL_C_CHAR;

        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
            return SQL_C_WCHAR;

        case SQL_DECIMAL:
            return SQL_C_CHAR;

        case SQL_NUMERIC:
            return SQL_C_NUMERIC;

        case SQL_BIT:                      return SQL_C_BIT;
        case SQL_TINYINT:                  return SQL_C_TINYINT;
        case SQL_SMALLINT:                 return SQL_C_SHORT;
        case SQL_INTEGER:                  return SQL_C_LONG;
        case SQL_BIGINT:                   return SQL_C_SBIGINT;
        case SQL_REAL:                     return SQL_C_FLOAT;

        case SQL_FLOAT:
        case SQL_DOUBLE:
            return SQL_C_DOUBLE;

        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
            return SQL_C_BINARY;

        case SQL_TYPE_DATE:                 return SQL_C_TYPE_DATE;
        case SQL_TYPE_TIME:                 return SQL_C_TYPE_TIME;
        case SQL_TYPE_TIMESTAMP:            return SQL_C_TYPE_TIMESTAMP;
        case SQL_INTERVAL_MONTH:            return SQL_C_INTERVAL_MONTH;
        case SQL_INTERVAL_YEAR:             return SQL_C_INTERVAL_YEAR;
        case SQL_INTERVAL_YEAR_TO_MONTH:    return SQL_C_INTERVAL_YEAR_TO_MONTH;
        case SQL_INTERVAL_DAY:              return SQL_C_INTERVAL_DAY;
        case SQL_INTERVAL_HOUR:             return SQL_C_INTERVAL_HOUR;
        case SQL_INTERVAL_MINUTE:           return SQL_C_INTERVAL_MINUTE;
        case SQL_INTERVAL_SECOND:           return SQL_C_INTERVAL_SECOND;
        case SQL_INTERVAL_DAY_TO_HOUR:      return SQL_C_INTERVAL_DAY_TO_HOUR;
        case SQL_INTERVAL_DAY_TO_MINUTE:    return SQL_C_INTERVAL_DAY_TO_MINUTE;
        case SQL_INTERVAL_DAY_TO_SECOND:    return SQL_C_INTERVAL_DAY_TO_SECOND;
        case SQL_INTERVAL_HOUR_TO_MINUTE:   return SQL_C_INTERVAL_HOUR_TO_MINUTE;
        case SQL_INTERVAL_HOUR_TO_SECOND:   return SQL_C_INTERVAL_HOUR_TO_SECOND;
        case SQL_INTERVAL_MINUTE_TO_SECOND: return SQL_C_INTERVAL_MINUTE_TO_SECOND;
        case SQL_GUID:                      return SQL_C_GUID;
    }

    return SQL_C_DEFAULT;
}

bool isVerboseType(SQLSMALLINT type) noexcept {
    switch (type) {
        case SQL_DATETIME:
        case SQL_INTERVAL:
            return true;
    }

    return false;
}

bool isConciseDateTimeIntervalType(SQLSMALLINT sql_type) noexcept {
    return (!isVerboseType(sql_type) && isVerboseType(tryConvertSQLTypeToVerboseType(sql_type)));
}

bool isConciseNonDateTimeIntervalType(SQLSMALLINT sql_type) noexcept {
    return !isVerboseType(tryConvertSQLTypeToVerboseType(sql_type));
}

SQLSMALLINT tryConvertSQLTypeToVerboseType(SQLSMALLINT type) noexcept {
    switch (type) {
        case SQL_TYPE_DATE:
        case SQL_TYPE_TIME:
        case SQL_TYPE_TIMESTAMP:
            return SQL_DATETIME;

        case SQL_INTERVAL_YEAR:
        case SQL_INTERVAL_MONTH:
        case SQL_INTERVAL_DAY:
        case SQL_INTERVAL_HOUR:
        case SQL_INTERVAL_MINUTE:
        case SQL_INTERVAL_SECOND:
        case SQL_INTERVAL_YEAR_TO_MONTH:
        case SQL_INTERVAL_DAY_TO_HOUR:
        case SQL_INTERVAL_DAY_TO_MINUTE:
        case SQL_INTERVAL_DAY_TO_SECOND:
        case SQL_INTERVAL_HOUR_TO_MINUTE:
        case SQL_INTERVAL_HOUR_TO_SECOND:
        case SQL_INTERVAL_MINUTE_TO_SECOND:
            return SQL_INTERVAL;
    }

    return type;
}

SQLSMALLINT convertSQLTypeToDateTimeIntervalCode(SQLSMALLINT type) noexcept {
    switch (type) {
        case SQL_TYPE_DATE:                 return SQL_CODE_DATE;
        case SQL_TYPE_TIME:                 return SQL_CODE_TIME;
        case SQL_TYPE_TIMESTAMP:            return SQL_CODE_TIMESTAMP;
        case SQL_INTERVAL_YEAR:             return SQL_CODE_YEAR;
        case SQL_INTERVAL_MONTH:            return SQL_CODE_MONTH;
        case SQL_INTERVAL_DAY:              return SQL_CODE_DAY;
        case SQL_INTERVAL_HOUR:             return SQL_CODE_HOUR;
        case SQL_INTERVAL_MINUTE:           return SQL_CODE_MINUTE;
        case SQL_INTERVAL_SECOND:           return SQL_CODE_SECOND;
        case SQL_INTERVAL_YEAR_TO_MONTH:    return SQL_CODE_YEAR_TO_MONTH;
        case SQL_INTERVAL_DAY_TO_HOUR:      return SQL_CODE_DAY_TO_HOUR;
        case SQL_INTERVAL_DAY_TO_MINUTE:    return SQL_CODE_DAY_TO_MINUTE;
        case SQL_INTERVAL_DAY_TO_SECOND:    return SQL_CODE_DAY_TO_SECOND;
        case SQL_INTERVAL_HOUR_TO_MINUTE:   return SQL_CODE_HOUR_TO_MINUTE;
        case SQL_INTERVAL_HOUR_TO_SECOND:   return SQL_CODE_HOUR_TO_SECOND;
        case SQL_INTERVAL_MINUTE_TO_SECOND: return SQL_CODE_MINUTE_TO_SECOND;
    }

    return 0;
}

SQLSMALLINT convertDateTimeIntervalCodeToSQLType(SQLSMALLINT code, SQLSMALLINT verbose_type) noexcept {
    switch (verbose_type) {
        case SQL_DATETIME:
            switch (code) {
                case SQL_CODE_DATE:             return SQL_TYPE_DATE;
                case SQL_CODE_TIME:             return SQL_TYPE_TIME;
                case SQL_CODE_TIMESTAMP:        return SQL_TYPE_TIMESTAMP;
            }
            break;

        case SQL_INTERVAL:
            switch (code) {
                case SQL_CODE_YEAR:             return SQL_INTERVAL_YEAR;
                case SQL_CODE_MONTH:            return SQL_INTERVAL_MONTH;
                case SQL_CODE_DAY:              return SQL_INTERVAL_DAY;
                case SQL_CODE_HOUR:             return SQL_INTERVAL_HOUR;
                case SQL_CODE_MINUTE:           return SQL_INTERVAL_MINUTE;
                case SQL_CODE_SECOND:           return SQL_INTERVAL_SECOND;
                case SQL_CODE_YEAR_TO_MONTH:    return SQL_INTERVAL_YEAR_TO_MONTH;
                case SQL_CODE_DAY_TO_HOUR:      return SQL_INTERVAL_DAY_TO_HOUR;
                case SQL_CODE_DAY_TO_MINUTE:    return SQL_INTERVAL_DAY_TO_MINUTE;
                case SQL_CODE_DAY_TO_SECOND:    return SQL_INTERVAL_DAY_TO_SECOND;
                case SQL_CODE_HOUR_TO_MINUTE:   return SQL_INTERVAL_HOUR_TO_MINUTE;
                case SQL_CODE_HOUR_TO_SECOND:   return SQL_INTERVAL_HOUR_TO_SECOND;
                case SQL_CODE_MINUTE_TO_SECOND: return SQL_INTERVAL_MINUTE_TO_SECOND;
            }
            break;
    };

    return SQL_UNKNOWN_TYPE;
}

bool isIntervalCode(SQLSMALLINT code) noexcept {
    switch (code) {
        case SQL_CODE_YEAR:
        case SQL_CODE_MONTH:
        case SQL_CODE_DAY:
        case SQL_CODE_HOUR:
        case SQL_CODE_MINUTE:
        case SQL_CODE_SECOND:
        case SQL_CODE_YEAR_TO_MONTH:
        case SQL_CODE_DAY_TO_HOUR:
        case SQL_CODE_DAY_TO_MINUTE:
        case SQL_CODE_DAY_TO_SECOND:
        case SQL_CODE_HOUR_TO_MINUTE:
        case SQL_CODE_HOUR_TO_SECOND:
        case SQL_CODE_MINUTE_TO_SECOND:
            return true;
    }

    return false;
}

bool intervalCodeHasSecondComponent(SQLSMALLINT code) noexcept {
    switch (code) {
        case SQL_CODE_SECOND:
        case SQL_CODE_DAY_TO_SECOND:
        case SQL_CODE_HOUR_TO_SECOND:
        case SQL_CODE_MINUTE_TO_SECOND:
            return true;
    }

    return false;
}

bool isInputParam(SQLSMALLINT param_io_type) noexcept {
    switch (param_io_type) {
        case SQL_PARAM_INPUT:
        case SQL_PARAM_INPUT_OUTPUT:
#if (ODBCVER >= 0x0380)
        case SQL_PARAM_INPUT_OUTPUT_STREAM:
#endif
            return true;
    }

    return false;
}

bool isOutputParam(SQLSMALLINT param_io_type) noexcept {
    switch (param_io_type) {
        case SQL_PARAM_OUTPUT:
        case SQL_PARAM_INPUT_OUTPUT:
#if (ODBCVER >= 0x0380)
        case SQL_PARAM_OUTPUT_STREAM:
        case SQL_PARAM_INPUT_OUTPUT_STREAM:
#endif
            return true;
    }

    return false;
}

bool isStreamParam(SQLSMALLINT param_io_type) noexcept {
#if (ODBCVER >= 0x0380)
    switch (param_io_type) {
        case SQL_PARAM_OUTPUT_STREAM:
        case SQL_PARAM_INPUT_OUTPUT_STREAM:
            return true;
    }
#endif

    return false;
}

std::string convertCTypeToDataSourceType(const BoundTypeInfo & type_info) {
    const auto set_nullability = [is_nullable = type_info.is_nullable] (const std::string & type_name) {
        return (is_nullable ? "Nullable(" + type_name + ")" : type_name);
    };

    std::string type_name;

    switch (type_info.c_type) {
        case SQL_C_WCHAR:
        case SQL_C_CHAR:
            type_name = set_nullability("String");
            break;

        case SQL_C_BIT:
            type_name = set_nullability("UInt8");
            break;

        case SQL_C_TINYINT:
        case SQL_C_STINYINT:
            type_name = set_nullability("Int8");
            break;

        case SQL_C_UTINYINT:
            type_name = set_nullability("UInt8");
            break;

        case SQL_C_SHORT:
        case SQL_C_SSHORT:
            type_name = set_nullability("Int16");
            break;

        case SQL_C_USHORT:
            type_name = set_nullability("UInt16");
            break;

        case SQL_C_LONG:
        case SQL_C_SLONG:
            type_name = set_nullability("Int32");
            break;

        case SQL_C_ULONG:
            type_name = set_nullability("UInt32");
            break;

        case SQL_C_SBIGINT:
            type_name = set_nullability("Int64");
            break;

        case SQL_C_UBIGINT:
            type_name = set_nullability("UInt64");
            break;

        case SQL_C_FLOAT:
            type_name = set_nullability("Float32");
            break;

        case SQL_C_DOUBLE:
            type_name = set_nullability("Float64");
            break;

        case SQL_C_NUMERIC:
            type_name = set_nullability("Decimal(" + std::to_string(type_info.precision) + ", " + std::to_string(type_info.scale) + ")");
            break;

        case SQL_C_BINARY:
            type_name = set_nullability(type_info.value_max_size > 0 ? ("FixedString(" + std::to_string(type_info.value_max_size) + ")") : "String");
            break;

        case SQL_C_GUID:
            type_name = set_nullability("UUID");
            break;

//      case SQL_C_BOOKMARK:
//      case SQL_C_VARBOOKMARK:

        case SQL_C_DATE:
        case SQL_C_TYPE_DATE:
            type_name = set_nullability("Date");
            break;

        case SQL_C_TIME:
        case SQL_C_TYPE_TIME:
            type_name = "LowCardinality(" + set_nullability("String") + ")";
            break;

        case SQL_C_TIMESTAMP:
        case SQL_C_TYPE_TIMESTAMP:
            type_name = set_nullability("DateTime");
            break;

        case SQL_C_INTERVAL_YEAR:
        case SQL_C_INTERVAL_MONTH:
        case SQL_C_INTERVAL_DAY:
        case SQL_C_INTERVAL_HOUR:
        case SQL_C_INTERVAL_MINUTE:
        case SQL_C_INTERVAL_SECOND:
        case SQL_C_INTERVAL_YEAR_TO_MONTH:
        case SQL_C_INTERVAL_DAY_TO_HOUR:
        case SQL_C_INTERVAL_DAY_TO_MINUTE:
        case SQL_C_INTERVAL_DAY_TO_SECOND:
        case SQL_C_INTERVAL_HOUR_TO_MINUTE:
        case SQL_C_INTERVAL_HOUR_TO_SECOND:
        case SQL_C_INTERVAL_MINUTE_TO_SECOND:
            type_name = "LowCardinality(" + set_nullability("String") + ")";
            break;
    }

    if (type_name.empty())
        throw std::runtime_error("Unable to deduce data source type from C type");

    return type_name;
}

std::string convertSQLTypeToDataSourceType(const BoundTypeInfo & type_info) {
    const auto set_nullability = [is_nullable = type_info.is_nullable] (const std::string & type_name) {
        return (is_nullable ? "Nullable(" + type_name + ")" : type_name);
    };

    std::string type_name;

    switch (type_info.sql_type) {
        case SQL_TYPE_NULL:
            type_name = set_nullability("Nothing");
            break;

        case SQL_WCHAR:
        case SQL_CHAR:
            type_name = set_nullability("String");
            break;

        case SQL_WVARCHAR:
        case SQL_VARCHAR:
            type_name = "LowCardinality(" + set_nullability("String") + ")";
            break;

        case SQL_WLONGVARCHAR:
        case SQL_LONGVARCHAR:
            type_name = set_nullability("String");
            break;

        case SQL_BIT:
            type_name = set_nullability("UInt8");
            break;

        case SQL_TINYINT:
            type_name = set_nullability("Int8");
            break;

        case SQL_SMALLINT:
            type_name = set_nullability("Int16");
            break;

        case SQL_INTEGER:
            type_name = set_nullability("Int32");
            break;

        case SQL_BIGINT:
            type_name = set_nullability("Int64");
            break;

        case SQL_REAL:
            type_name = set_nullability("Float32");
            break;

        case SQL_FLOAT:
        case SQL_DOUBLE:
            type_name = set_nullability("Float64");
            break;

        case SQL_DECIMAL:
        case SQL_NUMERIC:
            type_name = set_nullability("Decimal(" + std::to_string(type_info.precision) + ", " + std::to_string(type_info.scale) + ")");
            break;

        case SQL_BINARY:
            type_name = set_nullability(type_info.value_max_size > 0 ? ("FixedString(" + std::to_string(type_info.value_max_size) + ")") : "String");
            break;

        case SQL_VARBINARY:
            type_name = "LowCardinality(" + set_nullability("String") + ")";
            break;

        case SQL_LONGVARBINARY:
            type_name = set_nullability("String");
            break;

        case SQL_GUID:
            type_name = set_nullability("UUID");
            break;

        case SQL_TYPE_DATE:
            type_name = set_nullability("Date");
            break;

        case SQL_TYPE_TIME:
            type_name = "LowCardinality(" + set_nullability("String") + ")";
            break;

        case SQL_TYPE_TIMESTAMP:
            type_name = set_nullability("DateTime64(9)");
            break;

        case SQL_INTERVAL_MONTH:
        case SQL_INTERVAL_YEAR:
        case SQL_INTERVAL_YEAR_TO_MONTH:
        case SQL_INTERVAL_DAY:
        case SQL_INTERVAL_HOUR:
        case SQL_INTERVAL_MINUTE:
        case SQL_INTERVAL_SECOND:
        case SQL_INTERVAL_DAY_TO_HOUR:
        case SQL_INTERVAL_DAY_TO_MINUTE:
        case SQL_INTERVAL_DAY_TO_SECOND:
        case SQL_INTERVAL_HOUR_TO_MINUTE:
        case SQL_INTERVAL_HOUR_TO_SECOND:
        case SQL_INTERVAL_MINUTE_TO_SECOND:
            type_name = "LowCardinality(" + set_nullability("String") + ")";
            break;
    }

    if (type_name.empty())
        throw std::runtime_error("Unable to deduce data source type from SQL type");

    return type_name;
}

std::string convertSQLOrCTypeToDataSourceType(const BoundTypeInfo & type_info) {
    try {
        return convertSQLTypeToDataSourceType(type_info);
    }
    catch (...) {
        return convertCTypeToDataSourceType(type_info);
    }
}

bool isMappedToStringDataSourceType(SQLSMALLINT sql_type, SQLSMALLINT c_type) noexcept {
    switch (sql_type) {
        case SQL_WCHAR:
        case SQL_CHAR:
        case SQL_WVARCHAR:
        case SQL_VARCHAR:
        case SQL_WLONGVARCHAR:
        case SQL_LONGVARCHAR:
        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
        case SQL_TYPE_TIME:
        case SQL_INTERVAL_MONTH:
        case SQL_INTERVAL_YEAR:
        case SQL_INTERVAL_YEAR_TO_MONTH:
        case SQL_INTERVAL_DAY:
        case SQL_INTERVAL_HOUR:
        case SQL_INTERVAL_MINUTE:
        case SQL_INTERVAL_SECOND:
        case SQL_INTERVAL_DAY_TO_HOUR:
        case SQL_INTERVAL_DAY_TO_MINUTE:
        case SQL_INTERVAL_DAY_TO_SECOND:
        case SQL_INTERVAL_HOUR_TO_MINUTE:
        case SQL_INTERVAL_HOUR_TO_SECOND:
        case SQL_INTERVAL_MINUTE_TO_SECOND:
            return true;
    }

    switch (c_type) {
        case SQL_C_WCHAR:
        case SQL_C_CHAR:
        case SQL_C_BINARY:
        case SQL_C_TIME:
        case SQL_C_TYPE_TIME:
        case SQL_C_INTERVAL_YEAR:
        case SQL_C_INTERVAL_MONTH:
        case SQL_C_INTERVAL_DAY:
        case SQL_C_INTERVAL_HOUR:
        case SQL_C_INTERVAL_MINUTE:
        case SQL_C_INTERVAL_SECOND:
        case SQL_C_INTERVAL_YEAR_TO_MONTH:
        case SQL_C_INTERVAL_DAY_TO_HOUR:
        case SQL_C_INTERVAL_DAY_TO_MINUTE:
        case SQL_C_INTERVAL_DAY_TO_SECOND:
        case SQL_C_INTERVAL_HOUR_TO_MINUTE:
        case SQL_C_INTERVAL_HOUR_TO_SECOND:
        case SQL_C_INTERVAL_MINUTE_TO_SECOND:
            return true;
    }

    return false;
}
