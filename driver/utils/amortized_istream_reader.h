#pragma once

#include "driver/platform/platform.h"
#include "driver/utils/resize_without_initialization.h"

#include <algorithm>
#include <istream>
#include <stdexcept>
#include <string>
#include <cstring>

#include <Poco/Net/HTTPClientSession.h>

// A restricted wrapper around std::istream, that tries to reduce the number of std::istream::read() calls at the cost of extra std::memcpy().
// Maintains internal buffer of pre-read characters making AmortizedIStreamReader::read() calls for small counts more efficient.
// Handles incomplete reads and terminated std::istream more aggressively, by throwing exceptions.
class AmortizedIStreamReader
{
public:
    explicit AmortizedIStreamReader(std::istream & raw_stream, Poco::Net::HTTPClientSession & session)
        : raw_stream_(raw_stream), session_(session)
    {
    }

    ~AmortizedIStreamReader() {
        // Put back any pre-read characters, just in case...
        // (it should be done in reverse order)
        if (available() > 0) {
            for (auto it = buffer_.rbegin(); it < buffer_.rend() - offset_; ++it) {
                raw_stream_.putback(*it);
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

            auto * exception = session_.networkException();
            if (exception)
                throw std::runtime_error(exception->displayText());
            if (raw_stream_.bad())
                throw std::runtime_error("Unknown network error");
        }
    }

private:
    std::istream & raw_stream_;
    Poco::Net::HTTPClientSession & session_;
    std::size_t offset_ = 0;
    std::string buffer_;
};
