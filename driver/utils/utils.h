#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/unicode_conv.h"
#include "driver/utils/type_info.h"
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
#include <deque>
#include <functional>
#include <chrono>
#include <iomanip>
#include <set>
#include <sstream>
#include <thread>
#include <type_traits>
#include <vector>

#include <cstring>

class Environment;
class Connection;
class Descriptor;
class Statement;

template <typename T> constexpr auto & getObjectTypeName() { return "HANDLE"; }
template <> constexpr auto & getObjectTypeName<Environment>() { return "ENV"; }
template <> constexpr auto & getObjectTypeName<Connection>() { return "DBC"; }
template <> constexpr auto & getObjectTypeName<Descriptor>() { return "DESC"; }
template <> constexpr auto & getObjectTypeName<Statement>() { return "STMT"; }

template <typename T> constexpr int getObjectHandleType() { return 0; }
template <> constexpr int getObjectHandleType<Environment>() { return SQL_HANDLE_ENV; }
template <> constexpr int getObjectHandleType<Connection>() { return SQL_HANDLE_DBC; }
template <> constexpr int getObjectHandleType<Descriptor>() { return SQL_HANDLE_DESC; }
template <> constexpr int getObjectHandleType<Statement>() { return SQL_HANDLE_STMT; }

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

inline bool is_little_endian() noexcept {
    union {
        std::uint32_t i32;
        std::uint8_t i8[4];
    } i = { 0x89ABCDEF };

    return (i.i8[0] == 0xEF);
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

template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(const std::size_t max_size)
        : max_size_(max_size)
    {
    }

    void put(T && obj) {
        cache_.emplace_back(std::move(obj));
        while (cache_.size() > max_size_) {
            cache_.pop_front();
        }
    }

    T get() {
        if (cache_.empty()) {
            return T{};
        }
        else {
            T obj = std::move(cache_.front());
            cache_.pop_front();
            return obj;
        }
    }

private:
    const std::size_t max_size_;
    std::deque<T> cache_;
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

// Escapes a SQL literal value for using it in a single-quoted notation in a SQL query.
// Effectively, escapes ' and \ using \.
inline auto escapeForSQL(const std::string & literal) {
    std::string res;
    res.reserve(literal.size() + 10);
    for (auto ch : literal) {
        switch (ch) {
            case '\\':
            case '\'': {
                res += '\\';
                break;
            }
        }
        res += ch;
    }
    return res;
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
