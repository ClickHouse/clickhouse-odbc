#include "log.h"

#ifndef NDEBUG

std::ofstream logstream("/tmp/clickhouse-odbc.log", std::ios::out | std::ios::app);

#else 

decltype(std::cerr) & logstream = std::cerr;

#endif
