#pragma once

#include <string>

/** Replaces ODBC escape-sequence into a ClickHouse SQL-dialect.
 *
 * In case of error, input is returned as-is, with no modifications.
 */
std::string replaceEscapeSequences(const std::string & query);
