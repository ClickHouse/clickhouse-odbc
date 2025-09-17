#pragma once

#include <istream>
#include "Poco/BufferedStreamBuf.h"

#include <lz4.h>
#include <lz4frame.h>

class LZ4InflatingStreamBuf : public Poco::BufferedStreamBuf {
public:
    LZ4InflatingStreamBuf(const LZ4InflatingStreamBuf &) = delete;
    LZ4InflatingStreamBuf(LZ4InflatingStreamBuf &&) = delete;
    LZ4InflatingStreamBuf & operator=(const LZ4InflatingStreamBuf &) = delete;
    LZ4InflatingStreamBuf & operator=(LZ4InflatingStreamBuf &&) = delete;
    LZ4InflatingStreamBuf(std::istream & istr);

    ~LZ4InflatingStreamBuf();

    void reset() { }

protected:
    int readFromDevice(char * buffer, std::streamsize length);

private:
    std::istream * pIstr;

    /// place to keep incoming data
    std::vector<char> compressedBuffer;
    char * src_buffer;
    enum {
        /// proper buffer sizes are not required from functional standpoint, but should speed up processing
        STREAM_BUFFER_SIZE = (256 * 1024) + 4096, // CH uses 256k frames
        INFLATE_BUFFER_SIZE = 256 * 1024 * 4      // lets estimate ratio is 4
    };
    LZ4F_dctx * dctx;
    size_t bytes_left = 0;
};


/// Theses classes are needed to ensure the correct initialization
/// order of the stream buffer and base classes.
/// Exacltly the same as Poco::InflatingIOS and Poco:InflatingInputStream
template <typename StreamBufType>
class GenericInflatingIOS : public virtual std::ios {
public:
    GenericInflatingIOS(std::istream & istr) : buf(istr) {
        poco_ios_init(&buf);
    }


    ~GenericInflatingIOS() {};

protected:
    StreamBufType buf;
};


using LZ4InflatingIOS = GenericInflatingIOS<LZ4InflatingStreamBuf>;


template <typename IOSType>
class GenericInflatingInputStream : public std::istream, public IOSType {
public:
    using IOSType::buf;
    GenericInflatingInputStream(std::istream & istr) : std::istream(&buf), IOSType(istr) { }

    ~GenericInflatingInputStream() { }
};

using LZ4InflatingInputStream = GenericInflatingInputStream<LZ4InflatingIOS>;
