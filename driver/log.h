#pragma once

#include <iostream>
#include <fstream>

static bool log_enabled =
#ifndef NDEBUG
    true
#else
    false
#endif
    ;

#ifndef NDEBUG

#    if CMAKE_BUILD
#        include "config_cmake.h"
#    endif

#    if USE_DEBUG_17
#        include <iostream_debug_helpers.h>
#    endif
#endif

extern std::ofstream logstream;

#define LOG(message)                                                                 \
    do {                                                                             \
        if (log_enabled) {                                                           \
            logstream << __FILE__ << ":" << __LINE__ << " " << message << std::endl; \
        }                                                                            \
    } while (false)
