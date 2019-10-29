#include "driver/type_info.h"

#include <stdexcept>

const std::map<std::string, TypeInfo> types_g = {
    {"UInt8", TypeInfo {"TINYINT", true, SQL_TINYINT, 3, 1}},
    {"UInt16", TypeInfo {"SMALLINT", true, SQL_SMALLINT, 5, 2}},
    {"UInt32",
        TypeInfo {"INT",
            true,
            SQL_BIGINT /* was SQL_INTEGER */,
            10,
            4}}, // With perl, python ODBC drivers INT is uint32 and it cant store values bigger than 2147483647: 2147483648 -> -2147483648 4294967295 -> -1
    {"UInt32", TypeInfo {"INT", true, SQL_INTEGER, 10, 4}},
    {"UInt64", TypeInfo {"BIGINT", true, SQL_BIGINT, 20, 8}},
    {"Int8", TypeInfo {"TINYINT", false, SQL_TINYINT, 1 + 3, 1}}, // one char for sign
    {"Int16", TypeInfo {"SMALLINT", false, SQL_SMALLINT, 1 + 5, 2}},
    {"Int32", TypeInfo {"INT", false, SQL_INTEGER, 1 + 10, 4}},
    {"Int64", TypeInfo {"BIGINT", false, SQL_BIGINT, 1 + 19, 8}},
    {"Float32", TypeInfo {"REAL", false, SQL_REAL, 7, 4}},
    {"Float64", TypeInfo {"DOUBLE", false, SQL_DOUBLE, 15, 8}},
    {"Decimal", TypeInfo {"DECIMAL", false, SQL_DECIMAL, 1 + 2 + 38, 16}}, // -0.
    {"UUID", TypeInfo {"GUID", false, SQL_GUID, 8 + 1 + 4 + 1 + 4 + 1 + 4 + 12, sizeof(SQLGUID)}},
    {"String", TypeInfo {"TEXT", true, SQL_VARCHAR, TypeInfo::string_max_size, TypeInfo::string_max_size}},
    {"FixedString", TypeInfo {"TEXT", true, SQL_VARCHAR, TypeInfo::string_max_size, TypeInfo::string_max_size}},
    {"Date", TypeInfo {"DATE", true, SQL_TYPE_DATE, 10, 6}},
    {"DateTime", TypeInfo {"TIMESTAMP", true, SQL_TYPE_TIMESTAMP, 19, 16}},
    {"Array", TypeInfo {"TEXT", true, SQL_VARCHAR, TypeInfo::string_max_size, TypeInfo::string_max_size}},

    {"LowCardinality(String)",
        TypeInfo {"TEXT", true, SQL_VARCHAR, TypeInfo::string_max_size, TypeInfo::string_max_size}}, // todo: remove
    {"LowCardinality(FixedString)",
        TypeInfo {"TEXT", true, SQL_VARCHAR, TypeInfo::string_max_size, TypeInfo::string_max_size}}, // todo: remove
};

SQLSMALLINT convertSQLTypeToCType(SQLSMALLINT sql_type) noexcept {
    switch (sql_type) {
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
    std::string type_name;

    switch (type_info.c_type) {
        case SQL_C_WCHAR:
        case SQL_C_CHAR:
            type_name = "String";
            break;

        case SQL_C_BIT:
            type_name = "UInt8";
            break;

        case SQL_C_TINYINT:
        case SQL_C_STINYINT:
            type_name = "Int8";
            break;

        case SQL_C_UTINYINT:
            type_name = "UInt8";
            break;

        case SQL_C_SHORT:
        case SQL_C_SSHORT:
            type_name = "Int16";
            break;

        case SQL_C_USHORT:
            type_name = "UInt16";
            break;

        case SQL_C_LONG:
        case SQL_C_SLONG:
            type_name = "Int32";
            break;

        case SQL_C_ULONG:
            type_name = "UInt32";
            break;

        case SQL_C_SBIGINT:
            type_name = "Int64";
            break;

        case SQL_C_UBIGINT:
            type_name = "UInt64";
            break;

        case SQL_C_FLOAT:
            type_name = "Float32";
            break;

        case SQL_C_DOUBLE:
            type_name = "Float64";
            break;

        case SQL_C_NUMERIC:
            type_name = "Decimal(" + std::to_string(type_info.precision) + ", " + std::to_string(type_info.scale) + ")";
            break;

        case SQL_C_BINARY:
            type_name = (type_info.value_max_size > 0 ? ("FixedString(" + std::to_string(type_info.value_max_size) + ")") : "String");
            break;

        case SQL_C_GUID:
            type_name = "UUID";
            break;

//      case SQL_C_BOOKMARK:
//      case SQL_C_VARBOOKMARK:

        case SQL_C_DATE:
        case SQL_C_TYPE_DATE:
            type_name = "Date";
            break;

        case SQL_C_TIME:
        case SQL_C_TYPE_TIME:
            type_name = "LowCardinality(String)";
            break;

        case SQL_C_TIMESTAMP:
        case SQL_C_TYPE_TIMESTAMP:
            type_name = "DateTime";
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
            type_name = "LowCardinality(String)";
            break;
    }

    if (type_name.empty())
        throw std::runtime_error("Unable to deduce data source type from C type");

    if (type_info.is_nullable)
        type_name = "Nullable(" + type_name + ")";

    return type_name;
}

std::string convertSQLTypeToDataSourceType(const BoundTypeInfo & type_info) {
    std::string type_name;

    switch (type_info.sql_type) {
        case SQL_WCHAR:
        case SQL_CHAR:
            type_name = "String";
            break;

        case SQL_WVARCHAR:
        case SQL_VARCHAR:
            type_name = "LowCardinality(String)";
            break;

        case SQL_WLONGVARCHAR:
        case SQL_LONGVARCHAR:
            type_name = "String";
            break;

        case SQL_BIT:
            type_name = "UInt8";
            break;

        case SQL_TINYINT:
            type_name = "Int8";
            break;

        case SQL_SMALLINT:
            type_name = "Int16";
            break;

        case SQL_INTEGER:
            type_name = "Int32";
            break;

        case SQL_BIGINT:
            type_name = "Int64";
            break;

        case SQL_REAL:
            type_name = "Float32";
            break;

        case SQL_FLOAT:
        case SQL_DOUBLE:
            type_name = "Float64";
            break;

        case SQL_DECIMAL:
        case SQL_NUMERIC:
            type_name = "Decimal(" + std::to_string(type_info.precision) + ", " + std::to_string(type_info.scale) + ")";
            break;

        case SQL_BINARY:
            type_name = (type_info.value_max_size > 0 ? ("FixedString(" + std::to_string(type_info.value_max_size) + ")") : "String");
            break;

        case SQL_VARBINARY:
            type_name = "LowCardinality(String)";
            break;

        case SQL_LONGVARBINARY:
            type_name = "String";
            break;

        case SQL_GUID:
            type_name = "UUID";
            break;

        case SQL_TYPE_DATE:
            type_name = "Date";
            break;

        case SQL_TYPE_TIME:
            type_name = "LowCardinality(String)";
            break;

        case SQL_TYPE_TIMESTAMP:
            type_name = "DateTime";
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
            type_name = "LowCardinality(String)";
            break;
    }

    if (type_name.empty())
        throw std::runtime_error("Unable to deduce data source type from SQL type");

    if (type_info.is_nullable)
        type_name = "Nullable(" + type_name + ")";

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
