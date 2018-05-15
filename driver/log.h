#pragma once

#include <iostream>

#ifndef NDEBUG

#if CMAKE_BUILD
    #include "config_cmake.h"
#endif

#if USE_DEBUG_17
    #include <iostream_debug_helpers.h>
#endif

#define LOG(message) \
    do { std::cerr << __FILE__ << ":" << __LINE__ << " " << message << std::endl; } while (false)

#else 
#   define LOG(message)
#endif
