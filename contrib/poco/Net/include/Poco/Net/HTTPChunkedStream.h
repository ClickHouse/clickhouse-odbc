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
// This class has been heavily modified in order to solve issues related to incomplete
// steam of data, i.e. when server closes the connection before sending all the data.
// See https://github.com/pocoproject/poco/issues/5032
// Additionally it includes extra checks that the chunk size can be correctly parsed.
// This is part of larger PR:
// https://github.com/ClickHouse/clickhouse-odbc/pull/530
// --------------------------------------------------------------------------------- //

namespace Poco {
namespace Net {

POCO_DECLARE_EXCEPTION(Net_API, IncompleteChunkedTransfer, NetException)
POCO_DECLARE_EXCEPTION(Net_API, IncorrectChunkSize, NetException)

class HTTPSession;
class MessageHeader;


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
