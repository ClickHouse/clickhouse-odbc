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

// --------------------------- WARNING LOCAL CHANGES ------------------------------- //
// This class has been heavily modified to resolve issues related to incomplete
// streams of data, i.e., when the server closes the connection before sending all
// data.
// See https://github.com/pocoproject/poco/issues/5032
//
// Additionally, it implements handling of ClickHouse mid-stream exceptions.
// See https://github.com/ClickHouse/ClickHouse/issues/75175
//
// Unfortunately, due to the design of std::stream, an exception prevents the
// transmission of data that was successfully read before the exception occurred.
// Therefore, when an unexpected connection close occurs, the exception message
// from the stream will most likely never reach the client.
//
// For simplicity and predictability of mid-stream exception handling, this logic
// has also been implemented in this class. To achieve that, the last 32 KB of data
// is always cached in a lookback circular buffer. If the connection closes before
// the end of the stream, the buffer is checked for the exception marker.
// --------------------------------------------------------------------------------- //

namespace Poco {
namespace Net {

POCO_DECLARE_EXCEPTION(Net_API, IncompleteChunkedTransfer, NetException)
POCO_DECLARE_EXCEPTION(Net_API, IncorrectChunkSize, NetException)
POCO_DECLARE_EXCEPTION(Net_API, ClickHouseException, NetException)

class HTTPSession;
class MessageHeader;

class LookbackBuffer
    /// Circular buffer that maintains the last `buffer_size` bytes of a stream.
{
public:
	explicit LookbackBuffer(size_t buffer_size);

    /// commit data to the buffer
	void commit(const char * data, size_t data_size);

    /// Get last `buffer_size` committed to the buffer.
    /// If there were less than `buffer_size` bytes committed, the
    /// all the data will be returned. Must be used in conjunction
    /// with `size()`
	const char * data();

    /// Get the size of data stored in the buffer. If less than
    /// `buffer_size` bytes were committed to the buffer, the
    /// number of committed bytes will be returned, otherwise
    /// it will `buffer_size`.
	[[nodiscard]] size_t size() const;

private:
	std::vector<char> buffer;
	size_t count{0};
	size_t head{0};
};

class Net_API HTTPChunkedStreamBuf: public HTTPBasicStreamBuf
	/// This is the streambuf class used for reading and writing
	/// HTTP message bodies in chunked transfer coding.
{
public:
	using openmode = HTTPBasicStreamBuf::openmode;

	HTTPChunkedStreamBuf(HTTPSession& session, openmode mode, MessageHeader* pTrailer = nullptr);
	~HTTPChunkedStreamBuf();
	void close();

protected:
	int readFromDevice(char* buffer, std::streamsize length);
	int writeToDevice(const char* buffer, std::streamsize length);

private:
	int readChar();
	int readChunkEncodedStream(char* buffer, std::streamsize length);

	HTTPSession&    _session;
	openmode        _mode;
	std::streamsize _chunk;
	std::string     _chunkBuffer;
	MessageHeader*  _pTrailer;
	LookbackBuffer  _lookback_buffer;
};


class Net_API HTTPChunkedIOS: public virtual std::ios
	/// The base class for HTTPInputStream.
{
public:
	HTTPChunkedIOS(HTTPSession& session, HTTPChunkedStreamBuf::openmode mode, MessageHeader* pTrailer = nullptr);
	~HTTPChunkedIOS();
	HTTPChunkedStreamBuf* rdbuf();

protected:
	HTTPChunkedStreamBuf _buf;
};


class Net_API HTTPChunkedInputStream: public HTTPChunkedIOS, public std::istream
	/// This class is for internal use by HTTPSession only.
{
public:
	HTTPChunkedInputStream(HTTPSession& session, MessageHeader* pTrailer = nullptr);
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
	HTTPChunkedOutputStream(HTTPSession& session, MessageHeader* pTrailer = nullptr);
	~HTTPChunkedOutputStream();

	void* operator new(std::size_t size);
	void operator delete(void* ptr);

private:
	static Poco::MemoryPool _pool;
};


} } // namespace Poco::Net


#endif // Net_HTTPChunkedStream_INCLUDED
