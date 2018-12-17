#pragma once

#include <fstream>
#include <iostream>

extern bool log_enabled;

#ifndef NDEBUG

#    if CMAKE_BUILD
#        include "config_cmake.h"
#    endif

#    if USE_DEBUG_17
#        include <iostream_debug_helpers.h>
#    endif
#endif

#define LOG_DEFAULT_FILE "/tmp/clickhouse-odbc.log"
extern std::ofstream log_stream;
extern std::string log_file;

#define LOG(message)                                                                 \
    do {                                                                             \
        if (log_enabled)                                                             \
            log_stream << __FILE__ << ":" << __LINE__ << " " << message << std::endl; \
    } while (false)
