#include "lz4_inflating_input_stream.h"
#include "Poco/Exception.h"

#include "exception.h"

#include "driver.h"

LZ4InflatingStreamBuf::LZ4InflatingStreamBuf(std::istream & istr)
    : Poco::BufferedStreamBuf(INFLATE_BUFFER_SIZE, std::ios::in), pIstr(&istr) {
    LOG("LZ4InflatingInputStream ctor");
    size_t ret = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);

    if (LZ4F_isError(ret))
        throw SqlException("Cannot decompress data received from server");

    compressedBuffer.resize(INFLATE_BUFFER_SIZE);
}

LZ4InflatingStreamBuf::~LZ4InflatingStreamBuf() {
    /// seems Ok with repeatable calls, so no extra care needed
    LZ4F_freeDecompressionContext(dctx);
}

int LZ4InflatingStreamBuf::readFromDevice(char * dst_buffer, std::streamsize length) {
    LOG("LZ4InflatingInputStream::readFromDevice");

    if (!bytes_left) {
        if (!pIstr->good()) {
            LOG("LZ4InflatingInputStream::readFromDevice EOF or error (stream is not "
                "good)");
            return -1; // EOF or error
        }
        pIstr->read(compressedBuffer.data(), static_cast<std::streamsize>(compressedBuffer.size()));
        bytes_left = pIstr->gcount();
        src_buffer = compressedBuffer.data();
        LOG("LZ4InflatingInputStream::readFromDevice: bytes_left=" << bytes_left);
    }

    size_t bytesRead = bytes_left;

    if (!bytesRead) {
        LOG("LZ4InflatingInputStream::readFromDevice: No more compressed data");
        return 0; // No more compressed data
    }

    size_t decompressedSize = length;

    auto ret = LZ4F_decompress(dctx, dst_buffer, &decompressedSize, src_buffer, &bytesRead, nullptr);
    LOG("decompressedSize=" << decompressedSize << ", ret=" << ret << ", bytesRead=" << bytesRead);

    if (LZ4F_isError(ret)) {
        // LOG("throwing error " << std::string(
        //                              "LZ4 decompression failed. LZ4F version:") +
        //                              std::to_string(LZ4F_VERSION) +
        //                              "Error: " + LZ4F_getErrorName(ret));

        throw Poco::IOException(
            std::string("LZ4 decompression failed. LZ4F version:") + std::to_string(LZ4F_VERSION) + "Error: " + LZ4F_getErrorName(ret));
    }

    bytes_left -= bytesRead;
    src_buffer += bytesRead;

    return decompressedSize;
}
