#pragma once

#include "platform.h"

#include <string>

// An integer type big enough to hold the integer value that is built from all
// decimal digits of Decimal/Numeric values, as if there is no decimal point.
// Size of this integer defines the upper bound of the "info" the internal
// representation can carry.
// TODO: switch to 128-bit or even arbitrary-precision unsigned integer type.
using numeric_uint_container_t = std::uint64_t;

SQLSMALLINT convertSQLTypeToCType(SQLSMALLINT sql_type) noexcept;

bool isVerboseType(SQLSMALLINT type) noexcept;
bool isConciseDateTimeIntervalType(SQLSMALLINT sql_type) noexcept;
bool isConciseNonDateTimeIntervalType(SQLSMALLINT sql_type) noexcept;

SQLSMALLINT tryConvertSQLTypeToVerboseType(SQLSMALLINT type) noexcept;
SQLSMALLINT convertSQLTypeToDateTimeIntervalCode(SQLSMALLINT type) noexcept;
SQLSMALLINT convertDateTimeIntervalCodeToSQLType(SQLSMALLINT code, SQLSMALLINT verbose_type) noexcept;

bool isIntervalCode(SQLSMALLINT code) noexcept;
bool intervalCodeHasSecondComponent(SQLSMALLINT code) noexcept;

bool isInputParam(SQLSMALLINT param_io_type) noexcept;
bool isOutputParam(SQLSMALLINT param_io_type) noexcept;
bool isStreamParam(SQLSMALLINT param_io_type) noexcept;

struct BoundTypeInfo {
    SQLSMALLINT c_type = SQL_C_DEFAULT;
    SQLSMALLINT sql_type = SQL_UNKNOWN_TYPE;
    SQLLEN value_max_size = 0;
    std::int16_t precision = 0;
    std::int16_t scale = 0;
};

std::string convertCTypeToDataSourceType(const BoundTypeInfo & type_info);
std::string convertSQLTypeToDataSourceType(const BoundTypeInfo & type_info);
std::string convertCOrSQLTypeToDataSourceType(const BoundTypeInfo & type_info);
