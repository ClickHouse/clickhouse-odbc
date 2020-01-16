#pragma once

#include "driver/platform/platform.h"

#include <chrono>
#include <iostream>
#include <string>
#include <sstream>

#ifdef NDEBUG
#   define ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(test) test
#else
#   define ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(test) DISABLED_##test
#endif

#define START_MEASURING_TIME() \
    const auto start = std::chrono::system_clock::now();

#define STOP_MEASURING_TIME_AND_REPORT() \
    { \
        const auto end = std::chrono::system_clock::now(); \
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start); \
        std::stringstream str; \
        str << std::fixed << std::setprecision(9) << static_cast<double>(elapsed.count()) / static_cast<double>(1'000'000'000); \
        std::cout << "Executed in:\n\t" << str.str() << " seconds" << std::endl; \
    }
