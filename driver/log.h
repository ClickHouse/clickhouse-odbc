#pragma once

#include <iostream>

#ifndef NDEBUG

#define LOG(message) \
    do { std::cerr << __FILE__ << ":" << __LINE__ << " " << message << std::endl; } while (false)

#else 
#   define LOG(message)
#endif
