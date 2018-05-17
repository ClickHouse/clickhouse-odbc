#include "log.h"

#ifndef NDEBUG

std::ofstream logstream("/tmp/clickhouse-odbc.log", std::ios::out | std::ios::app);

#else 

const auto & logstream = std::cerr;

#endif
