#include "lz4_inflating_input_stream.h"
#include "Poco/Exception.h"

#include "exception.h"
#include <csignal>

#include "driver.h"

LZ4InflatingStreamBuf::LZ4InflatingStreamBuf(std::istream& istr)
  :Poco::BufferedStreamBuf(INFLATE_BUFFER_SIZE /* 4 * 1024 * 1024 */ /* STREAM_BUFFER_SIZE */, std::ios::in),
   pIstr(&istr) {
    LOG("LZ4InflatingInputStream ctor");
    size_t ret = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);

    if (LZ4F_isError(ret))
        throw SqlException("Cannot decompress data received from server");

    compressedBuffer.resize(INFLATE_BUFFER_SIZE);
}

LZ4InflatingStreamBuf::~LZ4InflatingStreamBuf()
{
    LZ4F_freeDecompressionContext(dctx);
}

#if 0
int LZ4InflatingInputStream::readFromDevice(char* buffer, std::streamsize length)
{
	if (_eof || !_pIstr) return 0;

	if (/*_zstr.*/avail_in == 0)
	{
		int n = 0;
		if (_pIstr->good())
		{
			_pIstr->read(_buffer, INFLATE_BUFFER_SIZE);
			n = static_cast<int>(_pIstr->gcount());
		}
		/*_zstr.*/next_in   = (unsigned char*) _buffer;
		/*_zstr.*/avail_in  = n;
	}
	/*_zstr.*/next_out  = (unsigned char*) buffer;
	/*_zstr.*/avail_out = static_cast<unsigned>(length);

  size_t ret;


	for (;;)
	{
		// int rc = inflate(&_zstr, Z_NO_FLUSH);
    size_t bytes_read = avail_in;
    size_t bytes_written = avail_out;


    ret = LZ4F_decompress(dctx, next_out, &bytes_written, next_in, &bytes_read, /* LZ4F_decompressOptions_t */ nullptr);

    if (LZ4F_isError(ret))
      throw Poco::IOException(std::string("LZ4 decompression failed. LZ4F version:") +  std::to_string(LZ4F_VERSION) + "Error: " + LZ4F_getErrorName(ret));

    if  (bytes_written == 0 || /* _pIstr->good() || */ !_pIstr->eof())
    {

		// if (rc == Z_DATA_ERROR && !_check)
		// {
		// 	if (/*_zstr.*/avail_in == 0)
		// 	{
		// 		if (_pIstr->good())
		// 			rc = Z_OK;
		// 		else
		// 			rc = Z_STREAM_END;
		// 	}
		// }
		// if (rc == Z_STREAM_END)
		// {
			_eof = true;
			return static_cast<int>(length) - /*_zstr.*/avail_out;
		}
		// if (rc != Z_OK) throw IOException(zError(rc));
		if (/*_zstr.*/avail_out == 0)
			return static_cast<int>(length);
		if (/*_zstr.*/avail_in == 0)
		{
			int n = 0;
			if (_pIstr->good())
			{
				_pIstr->read(_buffer, INFLATE_BUFFER_SIZE);
				n = static_cast<int>(_pIstr->gcount());
			}
			if (n > 0)
			{
				/*_zstr.*/next_in  = (unsigned char*) _buffer;
				/*_zstr.*/avail_in = n;
			}
			else return static_cast<int>(length) - /*_zstr.*/avail_out;
		}
	}
}
#endif

int LZ4InflatingStreamBuf::readFromDevice(char *dst_buffer,
                                          std::streamsize length) {
  LOG("LZ4InflatingInputStream::readFromDevice");

  if (!bytes_left) {
    if (!pIstr->good()) {
      LOG("LZ4InflatingInputStream::readFromDevice EOF or error (stream is not "
          "good)");
      return -1; // EOF or error
    }
    pIstr->read(compressedBuffer.data(),
                static_cast<std::streamsize>(compressedBuffer.size()));
    bytes_left = pIstr->gcount();
    src_buffer = compressedBuffer.data();
    LOG("LZ4InflatingInputStream::readFromDevice: bytes_left=" << bytes_left);
  }

  size_t bytesRead = bytes_left;
  LOG("LZ4InflatingInputStream::readFromDevice: shift="
      << src_buffer - compressedBuffer.data() << ", bytesRead=" << bytesRead);

  // size_t in_available = pIstr->gcount();

  if (bytesRead <= 0) // negative ??
  {
    LOG("LZ4InflatingInputStream::readFromDevice: No more compressed data");
    return 0; // No more compressed data
  }

  // Decompress the data into 'buffer'
  // Note: LZ4 requires you to know or guess the max decompressed size.
  // Here we assume 'length' is enough for decompressed data.

  // int decompressedSize = LZ4_decompress_safe(compressedBuffer.data(), buffer,
  // static_cast<int>(bytesRead), static_cast<int>(length));
  size_t decompressedSize = length;

  auto ret = LZ4F_decompress(dctx, dst_buffer, &decompressedSize, src_buffer,
                             &bytesRead, nullptr);
  LOG("decompressedSize=" << decompressedSize << ", ret=" << ret
                          << ", bytesRead=" << bytesRead);

  if (LZ4F_isError(ret)) {
    LOG("throwing error " << std::string(
                                 "LZ4 decompression failed. LZ4F version:") +
                                 std::to_string(LZ4F_VERSION) +
                                 "Error: " + LZ4F_getErrorName(ret));

    throw Poco::IOException(
        std::string("LZ4 decompression failed. LZ4F version:") +
        std::to_string(LZ4F_VERSION) + "Error: " + LZ4F_getErrorName(ret));
  }

  bytes_left -= bytesRead;
  src_buffer += bytesRead;

  return decompressedSize;
}

LZ4InflatingInputStream::LZ4InflatingInputStream(std::istream& istr):
	std::istream(&_buf),
	LZ4InflatingIOS(istr)
{
}


LZ4InflatingInputStream::~LZ4InflatingInputStream()
{
}


void LZ4InflatingInputStream::reset()
{
	_buf.reset();
	clear();
}

LZ4InflatingIOS::LZ4InflatingIOS(std::istream& istr):
	_buf(istr)
{
	poco_ios_init(&_buf);
}



LZ4InflatingIOS::~LZ4InflatingIOS()
{
}
