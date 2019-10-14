#pragma once

#include "platform.h"

#include <string>

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

std::string convertCTypeToDataSourceType(SQLSMALLINT C_type, std::size_t length);
std::string convertCOrSQLTypeToDataSourceType(SQLSMALLINT sql_type, std::size_t length);
