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

#if defined(_WIN32)
#    define LOG_DEFAULT_FILE "/temp/clickhouse-odbc.log"
#else
#    define LOG_DEFAULT_FILE "/tmp/clickhouse-odbc.log"
#endif

extern std::ofstream log_stream;
extern std::string log_file;
extern std::string log_header;

std::ostream & log_prefix(std::ofstream & stream);

#define LOG(message)                                                              \
    do {                                                                          \
        if (log_enabled) {                                                         \
            log_prefix(log_stream);                                               \
            log_stream << __FILE__ << ":" << __LINE__ << " " << message << std::endl; \
        } \
    } while (false)
