#pragma once

#include "driver/platform/platform.h"

#include <chrono>
#include <iostream>
#include <string>
#include <sstream>

#if defined(BUILD_TYPE_RELEASE)
#   define ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(test) test
#else
#   define ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(test) DISABLED_##test
#endif

#define START_MEASURING_TIME() \
    const auto start = std::chrono::system_clock::now();

#define STOP_MEASURING_TIME_AND_REPORT(iterations) \
    { \
        const auto end = std::chrono::system_clock::now(); \
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start); \
        const auto elapsed_sec = static_cast<double>(elapsed.count()) / static_cast<double>(1'000'000'000); \
        const auto elapsed_msec = static_cast<double>(elapsed.count()) / static_cast<double>(1'000'000); \
        std::cout << "\tTotal iterations: " << iterations << std::endl; \
        { \
            std::stringstream str; \
            str << std::fixed << std::setprecision(9) << elapsed_sec; \
            std::cout << "\tTotal time:       " << str.str() << " seconds" << std::endl; \
        } \
        { \
            std::stringstream str; \
            str << std::fixed << std::setprecision(3) << static_cast<double>(iterations) / elapsed_msec; \
            std::cout << "\tThroughput:       " << str.str() << " iterations per millisecond" << std::endl; \
        } \
        { \
            std::stringstream str; \
            str << std::fixed << std::setprecision(9) << elapsed_msec / static_cast<double>(iterations); \
            std::cout << "\tLatency:          " << str.str() << " milliseconds per iteration" << std::endl; \
        } \
    }
