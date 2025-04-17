#pragma once

#include "driver/platform/platform.h"
#include "driver/exception.h"

#if !defined(NDEBUG)
#   include "driver/utils/iostream_debug_helpers.h"
#endif

#include <Poco/NumberParser.h>
#include <Poco/String.h>
#include <Poco/UTF8String.h>

#ifdef _win_
#   include <processthreadsapi.h>
#else
#   include <sys/types.h>
#   include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

#define __STDC_WANT_LIB_EXT1__ 1

#include <cstring>
#include <ctime>

#ifdef _win_
#   define stack_alloc _alloca
#else
#   define stack_alloc alloca
#endif

class Environment;
class Connection;
class Descriptor;
class Statement;

template <typename T> constexpr auto & getObjectTypeName(); // Leave unimplemented for general case.
template <> constexpr auto & getObjectTypeName<Environment>() { return "ENV"; }
template <> constexpr auto & getObjectTypeName<Connection>() { return "DBC"; }
template <> constexpr auto & getObjectTypeName<Descriptor>() { return "DESC"; }
template <> constexpr auto & getObjectTypeName<Statement>() { return "STMT"; }

template <typename T> constexpr int getObjectHandleType(); // Leave unimplemented for general case.
template <> constexpr int getObjectHandleType<Environment>() { return SQL_HANDLE_ENV; }
template <> constexpr int getObjectHandleType<Connection>() { return SQL_HANDLE_DBC; }
template <> constexpr int getObjectHandleType<Descriptor>() { return SQL_HANDLE_DESC; }
template <> constexpr int getObjectHandleType<Statement>() { return SQL_HANDLE_STMT; }

template <typename CharType>
inline auto make_string_view(const CharType * src) {
    return std::basic_string_view<CharType>{src};
}

template <typename CharType>
inline auto make_string_view(const CharType * src, const std::size_t size) {
    return std::basic_string_view<CharType>{src, size};
}

template <typename CharType>
inline auto make_string_view(const std::basic_string<CharType> & src) {
    return std::basic_string_view<CharType>{src.c_str(), src.size()};
}

template <typename CharType>
inline auto make_raw_str(std::initializer_list<CharType> && list) {
    return std::string{list.begin(), list.end()};
}

template <typename T>
inline T fromString(const std::string & s) {
    T result;

    std::istringstream iss(s);
    iss >> result;

    if (iss.fail() || !iss.eof())
        throw std::runtime_error("bad lexical cast");

    return result;
}

template <typename T>
inline std::string toHexString(T n) {
    std::stringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << n;
    return stream.str();
}

inline auto getPID() {
#ifdef _win_
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

inline auto getTID() {
    return std::this_thread::get_id();
}

inline bool isLittleEndian() noexcept {
    union {
        std::uint32_t i32;
        std::uint8_t i8[4];
    } i = { 0x89ABCDEF };

    return (i.i8[0] == 0xEF);
}

inline void toLocalTime(const std::time_t & src, std::tm & dest) {
#ifdef _win_
    const auto err = localtime_s(&dest, &src);
#elif defined(__STDC_LIB_EXT1__)
    const auto err = localtime_s(&src, &dest);
#else
    auto * res = localtime_r(&src, &dest);
    const auto err = (res == &dest ? 0 : errno);
#endif

    if (err)
        throw std::runtime_error("Failed to convert time: " + std::string(std::strerror(err)));
}

inline bool isYes(std::string str) {
    Poco::trimInPlace(str);
    Poco::UTF8::toLowerInPlace(str);

    bool flag = false;
    return (Poco::NumberParser::tryParseBool(str, flag) ? flag : false);
}

inline bool isYesOrNo(std::string str) {
    Poco::trimInPlace(str);
    Poco::UTF8::toLowerInPlace(str);

    int flag_num = -1;
    if (Poco::NumberParser::tryParse(str, flag_num))
        return (flag_num == 0 || flag_num == 1);

    bool flag = false;
    return Poco::NumberParser::tryParseBool(str, flag);
}

inline auto tryStripParamPrefix(std::string param_name) {
    if (!param_name.empty() && param_name[0] == '@')
        param_name.erase(0, 1);
    return param_name;
}

struct UTF8CaseInsensitiveCompare {
    bool operator() (const std::string & lhs, const std::string & rhs) const {
        return Poco::UTF8::icompare(lhs, rhs) < 0;
    }
};

// Parses "Value List Arguments" of catalog functions.
// Effectively, parses a comma-separated list of possibly single-quoted values
// into a set of values. Escaping support is not supposed is such quoted values.
inline auto parseCatalogFnVLArgs(const std::string & value_list) {
    std::set<std::string> values;

    const auto value_list_mod = value_list + ',';
    std::string curr;
    int quotes = 0;

    for (auto ch : value_list_mod) {
        if (ch == ',' && quotes % 2 == 0) {
            Poco::trimInPlace(curr);
//          Poco::UTF8::toUpperInPlace(curr);
            if (curr.size() > 1 && curr.front() == '\'' && curr.back() == '\'') {
                curr.pop_back();
                curr.erase(0, 1);
            }
            values.emplace(std::move(curr));
            quotes = 0;
        }
        else {
            if (ch == '\'') {
                if (quotes >= 2)
                    throw std::runtime_error("Invalid syntax for catalog function value list argument: " + value_list);
                ++quotes;
            }
            curr += ch;
        }
    }

    if (!curr.empty() || quotes != 0)
        throw std::runtime_error("Invalid syntax for catalog function value list argument: " + value_list);

    return values;
}

// Checks whether a "Pattern Value Argument" matches any string, including an empty string.
// Effectively, checks if the string consists of 1 or more % chars only.
inline bool isMatchAnythingCatalogFnPatternArg(const std::string & pattern) {
    return (!pattern.empty() && pattern.find_first_not_of('%') == std::string::npos);
}

#define CASE_FALLTHROUGH(NAME) \
    case NAME:                 \
        if (!name)             \
            name = #NAME;

#define CASE_NUM(NAME, TYPE, VALUE)                                                                  \
    case NAME:                                                                                       \
        if (!name)                                                                                   \
            name = #NAME;                                                                            \
        LOG("GetInfo " << name << ", type: " << #TYPE << ", value: " << #VALUE << " = " << (VALUE)); \
        return fillOutputPOD<TYPE>(VALUE, out_value, out_value_length);
