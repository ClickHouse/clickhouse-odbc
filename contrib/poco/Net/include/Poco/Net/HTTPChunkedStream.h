//
// HTTPChunkedStream.h
//
// Library: Net
// Package: HTTP
// Module:  HTTPChunkedStream
//
// Definition of the HTTPChunkedStream class.
//
// Copyright (c) 2005-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef Net_HTTPChunkedStream_INCLUDED
#define Net_HTTPChunkedStream_INCLUDED


#include "Poco/Net/Net.h"
#include "Poco/Net/HTTPBasicStreamBuf.h"
#include "Poco/Net/NetException.h"
#include "Poco/MemoryPool.h"
#include <cstddef>
#include <istream>
#include <ostream>
#include <optional>
#include <unordered_map>

// --------------------------- WARNING LOCAL CHANGES ------------------------------- //
// This class has been heavily modified to resolve issues related to incomplete
// data streams — specifically, cases where the server closes the connection
// before sending all the data.
// See https://github.com/pocoproject/poco/issues/5032
//
// Additionally, it implements handling of ClickHouse mid-stream exceptions.
// See https://github.com/ClickHouse/ClickHouse/issues/75175
//
// Unfortunately, due to the design of std::stream, an exception prevents the
// transmission of data that was successfully read before the exception occurred.
// Therefore, when an unexpected connection closure occurs, the exception message
// from the stream will most likely never reach the client.
//
// For simplicity and predictability in mid-stream exception handling, this logic
// has also been implemented in this class. To achieve that, the class always
// prefetches extra data equal to the assumed maximum exception size. The class
// then always serves data from the prefetch buffer. Additionally, if the requested
// data size is small, the class fetches more data into the buffer to avoid extra
// read calls from the socket.
//
// In cases where the socket is closed without receiving the terminating
// zero-sized chunk, the class checks the contents of the prefetch buffer for
// the marker `__exception__\r\n`. If found, it throws a corresponding exception
// with the message that follows the marker.
// --------------------------------------------------------------------------------- //

namespace Poco {
namespace Net {

POCO_DECLARE_EXCEPTION(Net_API, DecompressionException, NetException)
POCO_DECLARE_EXCEPTION(Net_API, IncorrectSizeException, NetException)
POCO_DECLARE_EXCEPTION(Net_API, IncompleteChunkedTransferException, NetException)
POCO_DECLARE_EXCEPTION(Net_API, IncorrectChunkSizeException, NetException)
POCO_DECLARE_EXCEPTION(Net_API, ClickHouseException, NetException)

class HTTPSession;
class MessageHeader;

enum class HTTPCompressionType : uint8_t
{
	None,
	ZSTD
};

struct MemSpan
{
	char * data;
	size_t size;
};

class Net_API HTTPChunkedStreamBuf: public HTTPBasicStreamBuf
	/// This is the streambuf class used for reading and writing
	/// HTTP message bodies in chunked transfer coding.
{
public:
	using openmode = HTTPBasicStreamBuf::openmode;

	HTTPChunkedStreamBuf(
		HTTPSession& session,
		openmode mode,
		MessageHeader* pTrailer,
		std::unordered_map<std::string, std::string> headers
	);

	~HTTPChunkedStreamBuf();

	// Resets the prefetch buffer and sends the terminating bytes
	// if this is a write buffer.
	void close();

protected:
	// Returns data from the prefetch buffer.
	int readFromDevice(char* buffer, std::streamsize length) override;

	// Writes data to the socket (this function has not been modified).
	int writeToDevice(const char* buffer, std::streamsize length) override;

private:
	// Returns data from the prefetch buffer. Called by `readFromDevice`.
	int readFromDeviceImpl(char* buffer, std::streamsize length);

	// Copies data from the prefetch buffer to the destination buffer,
	// decompressing it along the way, if compression is enabled
	int transferFromPrefetchBuffer(char * buffer, std::streamsize length);

	// Fetches enough data to handle potential ClickHouse exceptions and fills
	// the prefetch buffer if it does not have `length` bytes available already.
	void prefetch(std::streamsize length);

	// Reads at most `length` bytes of data from the socket.
	int readDataFromSocket(char* buffer, std::streamsize length);

	// Reads a single character from the socket.
	int readCharFromSocket();

	// Checks whether the prefetch buffer contains a ClickHouse exception.
	std::optional<ClickHouseException> checkForClickHouseException();

private:
	// Since `_prefetchBuffer` is a ring buffer, data may wrap around the buffer
	// boundaries. These functions calculate contiguous memory ranges within the
	// buffer that can be read from or written to without wrapping.
	MemSpan readSpan();
	MemSpan writeSpan();
	void commitRead(size_t size);
	void commitWrite(size_t size);

private:
	class ZstdContext;

private:
	HTTPSession&    _session;
	openmode        _mode;
	std::streamsize _chunk;
	std::string     _chunkBuffer;
	MessageHeader*  _pTrailer;

	std::vector<char> _prefetchBuffer; // Ring buffer for data fetched from the socket
	size_t _prefetchBufferSize;        // Amount of data available in the buffer.
	size_t _prefetchBufferHead;        // Current read position in the buffer.
	bool _eof;                         // True if no more data is available in the socket.

	std::unordered_map<std::string, std::string> _headers;
	HTTPCompressionType _compression;
	std::unique_ptr<ZstdContext> _zstd_context; // Opaque wrapper around ZSTD_DStream to avoid `#import <zstd.h>` here

	bool _zstd_completed; // Marks the stream as correctly finished, we need to carry this state to the next
	                      // fetch operation, because the last operation that returns EOF would not give us any
	                      // data to call zstd correctly.
};


class Net_API HTTPChunkedIOS: public virtual std::ios
	/// The base class for HTTPInputStream.
{
public:
	HTTPChunkedIOS(
		HTTPSession& session,
		HTTPChunkedStreamBuf::openmode mode,
		MessageHeader* pTrailer,
		std::unordered_map<std::string, std::string> headers
	);
	~HTTPChunkedIOS();
	HTTPChunkedStreamBuf* rdbuf();

protected:
	HTTPChunkedStreamBuf _buf;
};


class Net_API HTTPChunkedInputStream: public HTTPChunkedIOS, public std::istream
	/// This class is for internal use by HTTPSession only.
{
public:
	HTTPChunkedInputStream(
		HTTPSession& session,
		MessageHeader* pTrailer,
		std::unordered_map<std::string, std::string> headers = {}
	);
	~HTTPChunkedInputStream();

	void* operator new(std::size_t size);
	void operator delete(void* ptr);

private:
	static Poco::MemoryPool _pool;
};


class Net_API HTTPChunkedOutputStream: public HTTPChunkedIOS, public std::ostream
	/// This class is for internal use by HTTPSession only.
{
public:
	HTTPChunkedOutputStream(
		HTTPSession& session,
		MessageHeader* pTrailer,
		std::unordered_map<std::string, std::string> headers = {}
	);
	~HTTPChunkedOutputStream();

	void* operator new(std::size_t size);
	void operator delete(void* ptr);

private:
	static Poco::MemoryPool _pool;
};


} } // namespace Poco::Net


#endif // Net_HTTPChunkedStream_INCLUDED
