#pragma once

#include <iostream>

#ifndef NDEBUG
    #include <fstream>

    #if CMAKE_BUILD
        #include "config_cmake.h"
    #endif

    #if USE_DEBUG_17
        #include <iostream_debug_helpers.h>
    #endif

    extern std::ofstream logstream;

    #define LOG(message) do { logstream << __FILE__ << ":" << __LINE__ << " " << message << std::endl; } while (false)

#else 

    extern decltype(std::cerr) & logstream;

    #define LOG(message)

#endif
