#pragma once

#include <iostream>

#ifndef NDEBUG

// TODO: #if...  #include <iostream_debug_helpers.h>

#define LOG(message) \
    do { std::cerr << __FILE__ << ":" << __LINE__ << " " << message << std::endl; } while (false)

#else 
#   define LOG(message)
#endif
