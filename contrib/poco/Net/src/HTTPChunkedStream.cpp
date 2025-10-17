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

POCO_IMPLEMENT_EXCEPTION(IncompleteChunkedTransfer, NetException, "Unexpected EOF in chunked encoding")
POCO_IMPLEMENT_EXCEPTION(IncorrectChunkSize, NetException, "Unable to parse the chunk size from the stream")

//
// HTTPChunkedStreamBuf
//


HTTPChunkedStreamBuf::HTTPChunkedStreamBuf(HTTPSession& session, openmode mode, MessageHeader* pTrailer):
	HTTPBasicStreamBuf(HTTPBufferAllocator::BUFFER_SIZE, mode),
	_session(session),
	_mode(mode),
	_chunk(0),
	_pTrailer(pTrailer)
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

int HTTPChunkedStreamBuf::readChar()
{
	static const int eof = std::char_traits<char>::eof();

	char buffer = 0;
	int n = _session.read(&buffer, 1);

	if (n == 0 && _session.peek() == eof) {
		throw IncompleteChunkedTransfer();
	}

	return buffer;
}

int HTTPChunkedStreamBuf::readFromDevice(char* buffer, std::streamsize length)
{
	try
	{
		return readChunkEncodedStream(buffer, length);
	}
	catch (const Exception & ex)
	{
		_session.setException(ex);
		throw;
	}
}

int HTTPChunkedStreamBuf::readChunkEncodedStream(char* buffer, std::streamsize length)
{
	static const int eof = std::char_traits<char>::eof();

	// read next chunk
	if (_chunk == 0)
	{
		int ch = readChar();

		// \r\n can be missing if this is the first chunk,
		// in this case \r\n\ is consumed by the header parser.
		// In this case we expect a hex digit right away. If
		// ch is not a hex digit, then we only can see \r\n
		// and nothing else.
		if (!Poco::Ascii::isHexDigit(ch)) {
			if (ch != '\r' || readChar() != '\n') {
				throw IncorrectChunkSize();
			}
			ch = readChar();
		}

		std::string chunkLen;
		while (Poco::Ascii::isHexDigit(ch) && chunkLen.size() < 8)
		{
			chunkLen += (char) ch;
			ch = readChar();
		}

		unsigned chunk = 0;

		// After we read a sequence of hex digits, we expect '\r\n'
		if (chunkLen.empty() || ch != '\r' || readChar() != '\n' || !NumberParser::tryParseHex(chunkLen, chunk)) {
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

		if (n == 0 && _session.peek() == eof) {
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
