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


#include <zstd.h>
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

POCO_IMPLEMENT_EXCEPTION(DecompressionException, NetException, "Failed to decompress the data");
POCO_IMPLEMENT_EXCEPTION(IncorrectSizeException, NetException, "Requested data of unexpected size")
POCO_IMPLEMENT_EXCEPTION(IncompleteChunkedTransferException, NetException, "Unexpected EOF in chunked encoding")
POCO_IMPLEMENT_EXCEPTION(IncorrectChunkSizeException, NetException, "Unable to parse the chunk size from the stream")
POCO_IMPLEMENT_EXCEPTION(ClickHouseException, NetException, "ClickHouse exception")

//
// HTTPChunkedStreamBuf
//

constexpr int eof = std::char_traits<char>::eof();

constexpr size_t min_look_ahead_size = 1024UL * 32;
constexpr size_t min_prefetch_size = std::max(min_look_ahead_size, (size_t)HTTPBufferAllocator::BUFFER_SIZE);
constexpr size_t min_read_buffer_capacity = min_prefetch_size + min_look_ahead_size;
constexpr size_t maximum_request_size = HTTPBufferAllocator::BUFFER_SIZE;

struct HTTPChunkedStreamBuf::ZstdContext
{
	std::unique_ptr<ZSTD_DStream, decltype(&ZSTD_freeDStream)> dstream{nullptr, ZSTD_freeDStream};
};

HTTPChunkedStreamBuf::HTTPChunkedStreamBuf(
	HTTPSession& session,
	openmode mode,
	MessageHeader* pTrailer,
	HTTPCompressionType compression
):
	HTTPBasicStreamBuf(HTTPBufferAllocator::BUFFER_SIZE, mode),
	_session(session),
	_mode(mode),
	_chunk(0),
	_pTrailer(pTrailer),
	_prefetchBuffer(min_read_buffer_capacity, '\0'),
	_prefetchBufferSize(0),
	_prefetchBufferHead(0),
	_eof(false),
	_compression(compression),
	_zstd_context(new ZstdContext{}),
	_zstd_completed(false)
{
	if (_compression == HTTPCompressionType::ZSTD) {
		_zstd_context->dstream.reset(ZSTD_createDStream());
		ZSTD_initDStream(_zstd_context->dstream.get());
	}
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
	} else {
		_prefetchBufferSize = 0;
		_prefetchBufferHead = 0;
		_prefetchBuffer.resize(min_read_buffer_capacity);
		_zstd_completed = false;
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
		_session.setException(ex);
		throw;
	}
	catch (const std::exception & ex)
	{
		auto poco_exception = Poco::Exception(ex.what());
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
		throw IncorrectSizeException(std::string("requested negative size of: ") + std::to_string(length));

	if (length > maximum_request_size)
		throw IncorrectSizeException(std::string("requested size is too large: ") + std::to_string(length));

	return transferFromPrefetchBuffer(buffer, length);
}


int HTTPChunkedStreamBuf::transferFromPrefetchBuffer(char * buffer, std::streamsize length)
{
	switch(_compression) {
		case HTTPCompressionType::None: {
			prefetch(length);

			if (!_prefetchBufferSize)
				return eof;

			if (_prefetchBufferSize < length)
				length = _prefetchBufferSize;

			memcpy(buffer, &_prefetchBuffer[_prefetchBufferHead], length);
			_prefetchBufferHead += length;
			_prefetchBufferSize -= length;
			return length;
		}
		case HTTPCompressionType::ZSTD: {
			ZSTD_outBuffer out = {buffer, static_cast<size_t>(length), 0};

			// Make sure we decompressed at least one byte to the input buffer.
			// ZSTD does not guarantee to produce output on each call, so we run
			// it in a cycle until we get some output data eventually.
			while (out.pos < length) {

				prefetch(std::max(ZSTD_DStreamInSize(), (size_t)length));

				// we reached EOF
				if (_prefetchBufferSize == 0) {
					if (out.pos)
						return out.pos; // flush what is read up to this moment
					if (_zstd_completed)
						return eof;	 // we are done if we had an empty zstd_res right before
					else
						throw DecompressionException(
							"Incomplete data, frame was truncated or connection closed prematurely");
				}

				// Note, this operation can, technically, read from the look ahead area, however
				// because the output buffer is smaller or equal to non-look-ahead area,
				// this is highly unlikely to happen, and even if it happens, it should read only
				// a small portion of the lookup area (<0.5% of non-look-area, i.e. around 655 bytes
				// out of 32Kb of the look ahead area)
				ZSTD_inBuffer in = {&_prefetchBuffer[_prefetchBufferHead], _prefetchBufferSize, 0};

				auto zstd_res = ZSTD_decompressStream(_zstd_context->dstream.get(), &out, &in);
				if (ZSTD_isError(zstd_res))
					throw DecompressionException(ZSTD_getErrorName(zstd_res));

				// Because we need to deliver the data, we need to keep last zstd status
				// to check if the stream was healthy at when we reach EOF
				_zstd_completed = (zstd_res == 0);

				_prefetchBufferHead += in.pos;
				_prefetchBufferSize -= in.pos;
			}

			return out.pos; // == length
		}
	}
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
 * if `length` is small, the function fetches extra data to avoid additional socket
 * reads in subsequent operations. Unless EOF is reached, the function is guaranteed
 * to fill the prefetch buffer with at least length + look ahead size.
 *
 * Note, the function introduces several performance reduction:
 * - Because it prefetches more data than needed, it holds the reader
 *   from consuming it when it is already available. However, the performance
 *   then improved by not touching the socket for subsequent calls.
 * - The active data is always moved to the beginning of the prefetch buffer,
 *   to give room for new data. A better way would be to use a ring buffer.
 */
void HTTPChunkedStreamBuf::prefetch(std::streamsize length)
{
	if (_eof)
		return;

	if (length + min_look_ahead_size <= _prefetchBufferSize)
		return;  // we already have data, no prefetch is needed

	// move unread data to the beginning
	memmove(&_prefetchBuffer[0], &_prefetchBuffer[_prefetchBufferHead], _prefetchBufferSize);
	_prefetchBufferHead = 0;

	if (length + min_look_ahead_size > _prefetchBuffer.size())
		_prefetchBuffer.resize(length + min_look_ahead_size);

	// amount of data still to be read from the socket
	size_t read_size = _prefetchBuffer.size() - _prefetchBufferSize;

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
	catch (const IncompleteChunkedTransferException & ex)
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
				throw IncorrectChunkSizeException();
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
			throw IncorrectChunkSizeException();
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
			throw IncompleteChunkedTransferException();
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
		throw IncompleteChunkedTransferException();
	}

	return buffer;
}

namespace {
// Function search for the clickhouse exception marker in the iterator range
// and returns the string that comes after that, otherwise nothing.
template <typename It>
std::optional<std::string> findClickHouseExceptionMessage(It begin, It end)
{
	const std::string_view exception_marker{"__exception__\r\n"};

	auto it = std::find_end(begin, end, exception_marker.begin(), exception_marker.end());
	if (it != end) {
		std::string message(it + exception_marker.size(), end);
		return message;
	}

	return std::nullopt;
}
} // anonymous namespace

/**
 * Scans the prefetch buffer for a ClickHouse exception marker.
 * Returns the corresponding exception if found.
 */
std::optional<ClickHouseException> HTTPChunkedStreamBuf::checkForClickHouseException()
{
	switch (_compression) {
		case HTTPCompressionType::None: {
			const char * begin = &_prefetchBuffer[_prefetchBufferHead];
			const char * end = &_prefetchBuffer[_prefetchBufferHead + _prefetchBufferSize];

			if (auto message = findClickHouseExceptionMessage(begin, end))
				return ClickHouseException(*message);

			return std::nullopt;
		}
		case HTTPCompressionType::ZSTD: {
			ZSTD_inBuffer in = {&_prefetchBuffer[_prefetchBufferHead], _prefetchBufferSize, 0};

			// ring buffer for uncompressed data
			std::vector<char> out_buffer(ZSTD_DStreamOutSize());
			size_t head = 0;

			// Protection from unlikely, but technically possible, infinite loop.
			// Zstd technically has it's own stalled loop protection, so we set this number high expecting
			// this fuse never trips.
			const size_t max_stalled_loop_steps = 500;
			size_t stalled_loop_steps = 0;
			while (stalled_loop_steps < max_stalled_loop_steps)
			{
				ZSTD_outBuffer out = {&out_buffer[head], out_buffer.size() - head, 0};

				auto res = ZSTD_decompressStream(_zstd_context->dstream.get(), &out, &in);

				if (ZSTD_isError(res)) {
					break; // we cannot go any further, but we should not throw anything here, to avoid
						   // overriding any existing exceptions. In this case we take what we already managed
						   // to decompress and work with it.
				}

				head += out.pos;

				// When zstd reaches end of its input, it might still have some data to write
				// to the output. If output buffer is full, we give zstd another chance
				// to write more to it by not exiting here right away.
				if (in.pos == in.size && head != out_buffer.size())
					break;
				// Note that if there is no output left and the data is aligned such as
				// it exactly equals the size of the output buffer, then setting pos to 0
				// at the next step would break the loop next time.

				if (head == out_buffer.size())
					head = 0;

				out.pos == 0 ? ++stalled_loop_steps : stalled_loop_steps = 0;
			}

			// When the buffer was not filled fully, it will move bunch of zeros to the
			// beginning of the buffer, ahead of actual data. This is exactly what we need.
			std::rotate(out_buffer.begin(), out_buffer.begin() + head, out_buffer.end());

			const char * begin = out_buffer.data();
			const char * end = out_buffer.data() + out_buffer.size();

			if (auto message = findClickHouseExceptionMessage(begin, end)) {
				return ClickHouseException(*message);
			}

			return std::nullopt;
		}
	}

}


//
// HTTPChunkedIOS
//


HTTPChunkedIOS::HTTPChunkedIOS(
	HTTPSession& session,
	HTTPChunkedStreamBuf::openmode mode,
	MessageHeader* pTrailer,
	HTTPCompressionType compression
):
	_buf(session, mode, pTrailer, compression)
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


HTTPChunkedInputStream::HTTPChunkedInputStream(
	HTTPSession& session,
	MessageHeader* pTrailer,
	HTTPCompressionType compression
):
	HTTPChunkedIOS(session, std::ios::in, pTrailer, compression),
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


HTTPChunkedOutputStream::HTTPChunkedOutputStream(
	HTTPSession& session,
	MessageHeader* pTrailer,
	HTTPCompressionType compression
):
	HTTPChunkedIOS(session, std::ios::out, pTrailer, compression),
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
