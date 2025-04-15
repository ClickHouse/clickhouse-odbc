#pragma once

#include "driver/platform/platform.h"

#include <Poco/Environment.h>

#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <sstream>

#include <cstdlib>
#include <cstring>

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

inline std::optional<std::string> get_env_var(const std::string & name) {
    if (Poco::Environment::has(name)) {
        return Poco::Environment::get("TZ");
    }

    return {};
}

inline void unsetEnvVar(const std::string & name) {
#ifdef _win_
    Poco::Environment::set(name, "");
#else
    if (unsetenv(name.c_str()) != 0) {
        throw std::runtime_error("Failed to unset environment variable " + name + ": " + std::strerror(errno));
    }
#endif
}

inline void setEnvVar(const std::string & name, const std::optional<std::string> & value) {
    if (value.has_value()) {
        Poco::Environment::set(name, value.value());
    }
    else {
        unsetEnvVar(name);
    }
}

inline bool compareOptionalSqlTimeStamps(std::optional<SQL_TIMESTAMP_STRUCT> a, std::optional<SQL_TIMESTAMP_STRUCT> b)
{
    if (!a && !b)
        return true;
    else if (!a || !b)
        return false;

    return
        a->year == b->year &&
        a->month == b->month &&
        a->day == b->day &&
        a->hour == b->hour &&
        a->minute == b->minute &&
        a->second == b->second &&
        a->fraction == b->fraction;
}

