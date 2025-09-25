#include "lz4_inflating_input_stream.h"
#include <assert.h>
#include "Poco/Exception.h"

#include "exception.h"

#include "driver.h"

LZ4InflatingStreamBuf::LZ4InflatingStreamBuf(std::istream & istr)
    : Poco::BufferedStreamBuf(INFLATE_BUFFER_SIZE, std::ios::in), pIstr(&istr) {
    LOG("LZ4InflatingInputStream ctor");
    size_t ret = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);

    if (LZ4F_isError(ret))
        throw SqlException("Cannot decompress data received from server");

    compressed_buffer.resize(STREAM_BUFFER_SIZE);
}

LZ4InflatingStreamBuf::~LZ4InflatingStreamBuf() {
    /// seems Ok with repeatable calls, so no extra care needed
    LZ4F_freeDecompressionContext(dctx);
}

int LZ4InflatingStreamBuf::readFromDevice(char * dst_buffer, std::streamsize length) {
    LOG("LZ4InflatingInputStream::readFromDevice");

    size_t decompressed_size = length;

    const size_t MAX_ATTEMPTS = 5;

    // Poco::BufferedStreamBuf consideres 0 as eof, so we need this internal loop
    //   to handle "decompression consumed no input but needs more" case
    for (size_t attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        if (!bytes_left || need_more_input) {
            if (!pIstr->good()) {
                LOG("LZ4InflatingInputStream::readFromDevice EOF or error (stream is not "
                    "good)");
                return -1;
            }
            if (need_more_input) {
                assert(bytes_left);
                if (compressed_buffer.size() <= bytes_left) {
                    throw Poco::IOException(std::string("LZ4 decompression failed, internal error (not enough room to create data chunk)"));
                }
                LOG("LZ4InflatingInputStream::readFromDevice: moving tail of " << bytes_left << " bytes (pretty rare case)");
                memmove(compressed_buffer.data(), src_buffer, bytes_left);
            }

            pIstr->read(compressed_buffer.data() + bytes_left, static_cast<std::streamsize>(compressed_buffer.size() - bytes_left));
            bytes_left += pIstr->gcount();
            src_buffer = compressed_buffer.data();
            LOG("LZ4InflatingInputStream::readFromDevice: bytes_left=" << bytes_left);
        }

        size_t bytes_read = bytes_left;

        if (!bytes_read) {
            LOG("LZ4InflatingInputStream::readFromDevice: No more compressed data");
            return 0; // No more compressed data
        }

        auto ret = LZ4F_decompress(dctx, dst_buffer, &decompressed_size, src_buffer, &bytes_read, nullptr);
        // Called too often, lets avoid
        // LOG("decompressedSize=" << decompressed_size << ", ret=" << ret << ", bytesRead=" << bytes_read);

        if (LZ4F_isError(ret)) {
            throw Poco::IOException(
                std::string("LZ4 decompression failed. LZ4F version:") + std::to_string(LZ4F_VERSION) + "Error: " + LZ4F_getErrorName(ret));
        }
        // ret is adverized as a 'hint for the best source size for next call, but seems useless
        //  other than to process error

        bytes_left -= bytes_read;
        src_buffer += bytes_read;

        // decompression consumed no input but needs more
        need_more_input = bytes_left && !bytes_read;

        if (decompressed_size)
            return decompressed_size;
    }
    LOG("LZ4InflatingInputStream::readFromDevice: MAX_ATTEMPTS reached");
    return -1;
}
