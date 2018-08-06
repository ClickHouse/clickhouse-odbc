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
    extern std::wofstream wlogstream;

    #define LOG(message) do { logstream << __FILE__ << ":" << __LINE__ << " " << message << std::endl; } while (false)
    #define WLOG(message) do { wlogstream << __FILE__ << ":" << __LINE__ << " " << message << std::endl; } while (false)

#else 

    extern decltype(std::cerr) & logstream;
    extern decltype(std::wcerr) & wlogstream;

    #define LOG(message)
    #define WLOG(message)

#endif
