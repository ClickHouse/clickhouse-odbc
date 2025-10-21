#pragma once

#include "Poco/BufferedStreamBuf.h"
#include <istream>

#include <lz4.h>
#include <lz4frame.h>

class LZ4InflatingStreamBuf : public Poco::BufferedStreamBuf
/// This stream decompresses all data passing through it
/// using zlib's inflate algorithm.
/// Example:
///     std::ifstream istr("data.gz", std::ios::binary);
///     InflatingInputStream inflater(istr, InflatingStreamBuf::STREAM_GZIP);
///     std::string data;
///     inflater >> data;
///
/// The underlying input stream can contain more than one gzip/deflate stream.
/// After a gzip/deflate stream has been processed, reset() can be called
/// to inflate the next stream.
{
public:
  LZ4InflatingStreamBuf(const LZ4InflatingStreamBuf &) = delete;
  LZ4InflatingStreamBuf(LZ4InflatingStreamBuf &&) = delete;
  LZ4InflatingStreamBuf &operator=(const LZ4InflatingStreamBuf &) = delete;
  LZ4InflatingStreamBuf &operator=(LZ4InflatingStreamBuf &&) = delete;
  LZ4InflatingStreamBuf(std::istream &istr);
  /// Creates an InflatingInputStream for expanding the compressed data read
  /// from the given input stream.

  ~LZ4InflatingStreamBuf();
  /// Destroys the InflatingInputStream.

  // int close();

  /// Finishes up the stream.
  ///
  /// Must be called when inflating to an output stream.

  void reset()  {
  }

  /// Resets the stream buffer.
protected:
    int readFromDevice(char * buffer, std::streamsize length);
    // int writeToDevice(const char * buffer, std::streamsize length);
    // int sync();

private:
    std::istream*  pIstr;
    std::vector<char> compressedBuffer;
    enum
    {
        STREAM_BUFFER_SIZE  = 1024,
        INFLATE_BUFFER_SIZE = 32768
    };
    LZ4F_dctx* dctx;
    // char*    _buffer;
    // std::istream*  _pIstr;
    // std::ostream*  _pOstr;
    // // z_stream _zstr;
    // bool     _eof;
    // bool     _check;

    // uint16_t     avail_in;  /* number of bytes available at next_in */
    // uint32_t    total_in;  /* total number of input bytes read so far */

    // unsigned char    *next_out; /* next output byte will go here */
    // unsigned char    *next_in; /* next output byte will go here */
    // uint16_t     avail_out; /* remaining free space at next_out */
    // uint32_t    total_out; /* total number of bytes output so far */
  size_t bytes_left = 0;
  char   * src_buffer;
};


class LZ4InflatingIOS: public virtual std::ios
	/// The base class for InflatingOutputStream and InflatingInputStream.
	///
	/// This class is needed to ensure the correct initialization
	/// order of the stream buffer and base classes.
{
public:
	LZ4InflatingIOS(std::istream& istr);
		/// Creates an LZ4InflatingIOS for expanding the compressed data read from
		/// the given input stream.
		///
		/// Please refer to the zlib documentation of inflateInit2() for a description
		/// of the windowBits parameter.

	~LZ4InflatingIOS();
		/// Destroys the LZ4InflatingIOS.

	LZ4InflatingStreamBuf* rdbuf();
		/// Returns a pointer to the underlying stream buffer.

protected:
	LZ4InflatingStreamBuf _buf;
};

class LZ4InflatingInputStream: public std::istream, public LZ4InflatingIOS
{
public:
	LZ4InflatingInputStream(std::istream& istr);
		/// Creates an InflatingInputStreamLZ4 for expanding the compressed data read from
		/// the given input stream.
		///
		/// Please refer to the zlib documentation of inflateInit2() for a description
		/// of the windowBits parameter.

	~LZ4InflatingInputStream();
		/// Destroys the InflatingInputStreamLZ4.

	void reset();
		/// Resets the zlib machinery so that another zlib stream can be read from
		/// the same underlying input stream.
};
