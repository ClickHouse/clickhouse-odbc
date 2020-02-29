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

#if defined(_MSC_VER) && _MSC_VER > 1916 // Not supported yet for Visual Studio 2019 and later.
#   define resize_without_initialization(container, size) container.resize(size)
#else
#   include <folly/memory/UninitializedMemoryHacks.h>
#   define resize_without_initialization(container, size) folly::resizeWithoutInitialization(container, size)
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

// A pool of at most max_size movable objects, that helps to reuse their capacity but not the content.
// Makes sense to use with std::string's and std::vector's to avoid reallocations of underlying storage.
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

// A restricted wrapper around std::istream, that tries to reduce the number of std::istream::read() calls at the cost of extra std::memcpy().
// Maintains internal buffer of pre-read characters making AmortizedIStreamReader::read() calls for small counts more efficient.
// Handles incomplete reads and terminated std::istream more aggressively, by throwing exceptions.
class AmortizedIStreamReader
{
public:
    explicit AmortizedIStreamReader(std::istream & raw_stream)
        : raw_stream_(raw_stream)
    {
    }

    ~AmortizedIStreamReader() {
        // Put back any pre-read characters, just in case...
        if (available() > 0) {
            for (std::size_t i = buffer_.size() - 1; i >= offset_; --i) {
                raw_stream_.putback(buffer_[i]);
            }
        }
    }

    AmortizedIStreamReader(const AmortizedIStreamReader &) = delete;
    AmortizedIStreamReader(AmortizedIStreamReader &&) noexcept = delete;
    AmortizedIStreamReader & operator= (const AmortizedIStreamReader &) = delete;
    AmortizedIStreamReader & operator= (AmortizedIStreamReader &&) noexcept = delete;

    bool eof() {
        if (available() > 0)
            return false;

        if (raw_stream_.eof() || raw_stream_.fail())
            return true;

        tryPrepare(1);

        if (available() > 0)
            return false;

        return (raw_stream_.eof() || raw_stream_.fail());
    }

    char get() {
        tryPrepare(1);

        if (available() < 1)
            throw std::runtime_error("Incomplete input stream, expected at least 1 more byte");

        return buffer_[offset_++];
    }

    AmortizedIStreamReader & read(char * str, std::size_t count) {
        tryPrepare(count);

        if (available() < count)
            throw std::runtime_error("Incomplete input stream, expected at least " + std::to_string(count) + " more bytes");

        if (str) // If str == nullptr, just silently consume requested amount of data.
            std::memcpy(str, &buffer_[offset_], count);

        offset_ += count;

        return *this;
    }

private:
    std::size_t available() const {
        if (offset_ < buffer_.size())
            return (buffer_.size() - offset_);

        return 0;
    }

    void tryPrepare(std::size_t count) {
        const auto avail = available();

        if (avail < count) {
            static constexpr std::size_t min_read_size = 1 << 13; // 8 KB

            const auto to_read = std::max<std::size_t>(min_read_size, count - avail);
            const auto tail_capacity = buffer_.capacity() - buffer_.size();
            const auto free_capacity = tail_capacity + offset_;

            if (tail_capacity < to_read) { // Reallocation or at least compacting have to be done.
                if (free_capacity < to_read) { // Reallocation is unavoidable. Compact the buffer while doing it.
                    if (avail > 0) {
                        decltype(buffer_) tmp;
                        resize_without_initialization(tmp, avail + to_read);
                        std::memcpy(&tmp[0], &buffer_[offset_], avail);
                        buffer_.swap(tmp);
                    }
                    else {
                        buffer_.clear();
                        resize_without_initialization(buffer_, to_read);
                    }
                }
                else { // Compacting the buffer is enough.
                    std::memmove(&buffer_[0], &buffer_[offset_], avail);
                    resize_without_initialization(buffer_, avail + to_read);
                }
                offset_ = 0;
            }
            else {
                resize_without_initialization(buffer_, buffer_.size() + to_read);
            }

            raw_stream_.read(&buffer_[offset_ + avail], to_read);

            if (raw_stream_.gcount() < to_read)
                buffer_.resize(buffer_.size() - (to_read - raw_stream_.gcount()));
        }
    }

private:
    std::istream & raw_stream_;
    std::size_t offset_ = 0;
    std::string buffer_;
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
