#pragma once

#include <iostream>

#ifndef NDEBUG

#define LOG(message) \
    do { std::cerr << message << std::endl; } while (false)

#else 
#   define LOG(message)
#endif
