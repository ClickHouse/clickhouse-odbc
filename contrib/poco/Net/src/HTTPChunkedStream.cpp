//
// HTTPChunkedStream.cpp
//
// Library: Net
// Package: HTTP
// Module:  HTTPChunkedStream
//
// Copyright (c) 2005-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/Net/HTTPChunkedStream.h"
#include "Poco/Net/HTTPHeaderStream.h"
#include "Poco/Net/HTTPSession.h"
#include "Poco/NumberFormatter.h"
#include "Poco/NumberParser.h"
#include "Poco/Ascii.h"


using Poco::NumberFormatter;
using Poco::NumberParser;


namespace Poco {
namespace Net {

POCO_IMPLEMENT_EXCEPTION(IncorrectSize, NetException, "Requested data of unexpected size")
POCO_IMPLEMENT_EXCEPTION(IncompleteChunkedTransfer, NetException, "Unexpected EOF in chunked encoding")
POCO_IMPLEMENT_EXCEPTION(IncorrectChunkSize, NetException, "Unable to parse the chunk size from the stream")
POCO_IMPLEMENT_EXCEPTION(ClickHouseException, NetException, "ClickHouse exception")

//
// HTTPChunkedStreamBuf
//

constexpr int eof = std::char_traits<char>::eof();

constexpr size_t min_look_ahead_size = 1024UL * 32;
constexpr size_t min_prefetch_size = 1024UL * 32;
constexpr size_t read_buffer_capacity = min_prefetch_size + min_look_ahead_size;
constexpr size_t maximum_request_size = 1024 * 1024 * 2;


HTTPChunkedStreamBuf::HTTPChunkedStreamBuf(HTTPSession& session, openmode mode, MessageHeader* pTrailer):
	HTTPBasicStreamBuf(HTTPBufferAllocator::BUFFER_SIZE, mode),
	_session(session),
	_mode(mode),
	_chunk(0),
	_pTrailer(pTrailer),
	_prefetchBuffer(read_buffer_capacity, '\0'),
	_prefetchBufferSize(0),
	_prefetchBufferHead(0),
	_eof(false)
{
}


HTTPChunkedStreamBuf::~HTTPChunkedStreamBuf()
{
}


void HTTPChunkedStreamBuf::close()
{
	if (_mode & std::ios::out)
	{
		sync();
		if (_pTrailer && !_pTrailer->empty())
		{
			HTTPHeaderOutputStream hos(_session);
			hos.write("0\r\n", 3);
			_pTrailer->write(hos);
			hos.write("\r\n", 2);
		}
		else
		{
			_session.write("0\r\n\r\n", 5); // If possible, send in one write
		}
	}
}

/**
 * This virtual function is used by Poco's BasicBufferedStreamBuf to read data.
 * It only wraps exception handling, the actual work is performed by `readFromDeviceImpl()`.
 * All exceptions are caught and then silenced by std::istream implementation. The only thing
 * that is left to the user is the bad bid. Poco provides an extra mechanism to
 * pass exceptions to the caller: HTTPSession::setException() and HTTPSession::getException().
 * We use it here to pass exceptions to the caller.
 */
int HTTPChunkedStreamBuf::readFromDevice(char* buffer, std::streamsize length)
{
	try
	{
		return readFromDeviceImpl(buffer, length);
	}
	catch (const Poco::Exception & ex)
	{
		reset();
		_session.setException(ex);
		throw;
	}
	catch (const std::exception & ex)
	{
		auto poco_exception = Poco::Exception(ex.what());
		reset();
		_session.setException(poco_exception);
		throw poco_exception;
	}
}

/**
 * This virtual function is used by Poco's BasicBufferedStreamBuf to read data.
 * It returns the number of bytes read. On EOF, the function returns `eof` instead of 0.
 *
 * The function does not read data directly; instead, it serves data from the
 * prefetch buffer. To ensure that sufficient data is available, it first calls `prefetch()`.
 */
int HTTPChunkedStreamBuf::readFromDeviceImpl(char* buffer, std::streamsize length)
{
	if (length == 0)
		return 0;

	if (length < 0)
		throw IncorrectSize(std::string("requested negative size of: ") + std::to_string(length));

	if (length > maximum_request_size)
		throw IncorrectSize(std::string("requested size is too large: ") + std::to_string(length));

	if (!_eof)
		prefetch(length);

	if (_prefetchBufferSize < length)
		length = _prefetchBufferSize;

	if (!length) {
		reset();
		return eof;
	}

	memcpy(buffer, &_prefetchBuffer[_prefetchBufferHead], length);
	_prefetchBufferHead += length;
	_prefetchBufferSize -= length;

	return length;
}


/**
 * Writes data to the socket.
 * This function has not been modified.
 */
int HTTPChunkedStreamBuf::writeToDevice(const char* buffer, std::streamsize length)
{
	_chunkBuffer.clear();
	NumberFormatter::appendHex(_chunkBuffer, length);
	_chunkBuffer.append("\r\n", 2);
	_chunkBuffer.append(buffer, static_cast<std::string::size_type>(length));
	_chunkBuffer.append("\r\n", 2);
	_session.write(_chunkBuffer.data(), static_cast<std::streamsize>(_chunkBuffer.size()));
	return static_cast<int>(length);
}

/**
 * Fetches enough data to handle potential ClickHouse exceptions. For efficiency,
 * if `size` is small, the function fetches extra data to avoid additional socket
 * reads in subsequent operations.
 */
void HTTPChunkedStreamBuf::prefetch(std::streamsize length)
{
	if (length + min_look_ahead_size <= _prefetchBufferSize)
		return;  // we already have data, no prefetch is needed

	// move unread data to the beginning
	memmove(&_prefetchBuffer[0], &_prefetchBuffer[_prefetchBufferHead], _prefetchBufferSize);
	_prefetchBufferHead = 0;

	// amount of data still to be read from the socket
	size_t read_size = read_buffer_capacity - _prefetchBufferSize;

	try
	{
		size_t total_read = 0;
		while (total_read < read_size) {
			int res = readDataFromSocket(&_prefetchBuffer[_prefetchBufferSize], read_size - total_read);
			if (res == eof) {
				_eof = true;
				return;
			}
			total_read += res;
			_prefetchBufferSize += res;
		}
	}
	catch (const IncompleteChunkedTransfer & ex)
	{
		auto ch_ex = checkForClickHouseException();

		if (ch_ex)
			throw *ch_ex;
		throw;
	}
}

int HTTPChunkedStreamBuf::readDataFromSocket(char* buffer, std::streamsize length)
{
	// read next chunk
	if (_chunk == 0)
	{
		int ch = readCharFromSocket();

		// the "\r\n" sequence may be missing if this is the first chunk.
		// In that case, "\r\n" is consumed by the header parser,
		// and we expect a hexadecimal digit immediately afterward.
		// If `ch` is not a hex digit, then the only valid sequence we can see
		// is "\r\n" — nothing else.
		if (!Poco::Ascii::isHexDigit(ch)) {
			if (ch != '\r' || readCharFromSocket() != '\n') {
				throw IncorrectChunkSize();
			}
			ch = readCharFromSocket();
		}

		std::string chunkLen;
		while (Poco::Ascii::isHexDigit(ch) && chunkLen.size() < 8)
		{
			chunkLen += (char) ch;
			ch = readCharFromSocket();
		}

		unsigned chunk = 0;

		// after we read a sequence of hex digits, we expect '\r\n'
		if (chunkLen.empty() || ch != '\r' || readCharFromSocket() != '\n' || !NumberParser::tryParseHex(chunkLen, chunk)) {
			throw IncorrectChunkSize();
		}

		_chunk = static_cast<std::streamsize>(chunk);
	}

	// chunk has data - read it to the buffer
	if (_chunk > 0)
	{
		if (length > _chunk) length = _chunk;
		int n = _session.read(buffer, length);

		if (n > 0) _chunk -= n;

		if (n == 0 && length > 0) {
			throw IncompleteChunkedTransfer();
		}

		return n;
	}

	// last chunk
	else if (_chunk == 0)
	{
		int ch = _session.peek();
		if (ch != eof && ch != '\r' && ch != '\n')
		{
			HTTPHeaderInputStream his(_session);
			if (_pTrailer)
			{
				_pTrailer->read(his);
			}
			else
			{
				MessageHeader trailer;
				trailer.read(his);
			}
		}
		else
		{
			ch = _session.get();
			while (ch != eof && ch != '\n') ch = _session.get();
		}
		_chunk = -1;
		return 0;
	}
	else return eof;
}

int HTTPChunkedStreamBuf::readCharFromSocket()
{
	char buffer = 0;
	int n = _session.read(&buffer, 1);

	if (n == 0) {
		throw IncompleteChunkedTransfer();
	}

	return buffer;
}

/**
 * Scans the prefetch buffer for a ClickHouse exception marker.
 * Returns the corresponding exception if found.
 */
std::optional<ClickHouseException> HTTPChunkedStreamBuf::checkForClickHouseException()
{
	const std::string_view exception_marker{"__exception__\r\n"};
	const char * begin = &_prefetchBuffer[_prefetchBufferHead];
	const char * end = &_prefetchBuffer[_prefetchBufferHead + _prefetchBufferSize];

	auto pos = std::find_end(begin, end, exception_marker.begin(), exception_marker.end());
	if (pos != end) {
		std::string message(pos + exception_marker.size(), end);
		return ClickHouseException(message);
	}

	return std::nullopt;
}

void HTTPChunkedStreamBuf::reset()
{
	_prefetchBufferSize = 0;
	_prefetchBufferHead = 0;
	_prefetchBuffer.resize(read_buffer_capacity);
}


//
// HTTPChunkedIOS
//


HTTPChunkedIOS::HTTPChunkedIOS(HTTPSession& session, HTTPChunkedStreamBuf::openmode mode, MessageHeader* pTrailer):
	_buf(session, mode, pTrailer)
{
	poco_ios_init(&_buf);
}


HTTPChunkedIOS::~HTTPChunkedIOS()
{
	try
	{
		_buf.close();
	}
	catch (...)
	{
	}
}


HTTPChunkedStreamBuf* HTTPChunkedIOS::rdbuf()
{
	return &_buf;
}


//
// HTTPChunkedInputStream
//


Poco::MemoryPool HTTPChunkedInputStream::_pool(sizeof(HTTPChunkedInputStream));


HTTPChunkedInputStream::HTTPChunkedInputStream(HTTPSession& session, MessageHeader* pTrailer):
	HTTPChunkedIOS(session, std::ios::in, pTrailer),
	std::istream(&_buf)
{
}


HTTPChunkedInputStream::~HTTPChunkedInputStream()
{
}


void* HTTPChunkedInputStream::operator new(std::size_t size)
{
	return _pool.get();
}


void HTTPChunkedInputStream::operator delete(void* ptr)
{
	try
	{
		_pool.release(ptr);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


//
// HTTPChunkedOutputStream
//


Poco::MemoryPool HTTPChunkedOutputStream::_pool(sizeof(HTTPChunkedOutputStream));


HTTPChunkedOutputStream::HTTPChunkedOutputStream(HTTPSession& session, MessageHeader* pTrailer):
	HTTPChunkedIOS(session, std::ios::out, pTrailer),
	std::ostream(&_buf)
{
}


HTTPChunkedOutputStream::~HTTPChunkedOutputStream()
{
}


void* HTTPChunkedOutputStream::operator new(std::size_t size)
{
	return _pool.get();
}


void HTTPChunkedOutputStream::operator delete(void* ptr)
{
	try
	{
		_pool.release(ptr);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


} } // namespace Poco::Net
