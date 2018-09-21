#include "log.h"

std::ofstream logstream("/tmp/clickhouse-odbc.log", std::ios::out | std::ios::app);
