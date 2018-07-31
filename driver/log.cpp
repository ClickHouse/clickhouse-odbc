#include "log.h"

#ifndef NDEBUG

std::ofstream logstream("/tmp/clickhouse-odbc.log", std::ios::out | std::ios::app);
std::wofstream wlogstream("/tmp/clickhouse-odbcw.log", std::ios::out | std::ios::app);

#else

decltype(std::cerr) & logstream = std::cerr;
decltype(std::wcerr) & wlogstream = std::wcerr;

#endif
