#pragma once

#include "platform.h"

#include <string>

SQLSMALLINT convert_sql_type_to_C_type(SQLSMALLINT sql_type) noexcept;

bool is_verbose_type(SQLSMALLINT type) noexcept;
bool is_concise_datetime_interval_type(SQLSMALLINT sql_type) noexcept;
bool is_concise_non_datetime_interval_type(SQLSMALLINT sql_type) noexcept;

SQLSMALLINT try_convert_sql_type_to_verbose_type(SQLSMALLINT type) noexcept;
SQLSMALLINT convert_sql_type_to_datetime_interval_code(SQLSMALLINT type) noexcept;
SQLSMALLINT convert_datetime_interval_code_to_sql_type(SQLSMALLINT code, SQLSMALLINT verbose_type) noexcept;

bool is_interval_code(SQLSMALLINT code) noexcept;
bool interval_code_has_second_component(SQLSMALLINT code) noexcept;

bool is_input_param(SQLSMALLINT param_io_type);
bool is_output_param(SQLSMALLINT param_io_type);
bool is_stream_param(SQLSMALLINT param_io_type);

std::string convert_C_type_to_data_source_type(SQLSMALLINT C_type, std::size_t length);
std::string convert_C_or_sql_type_to_data_source_type(SQLSMALLINT sql_type, std::size_t length);
