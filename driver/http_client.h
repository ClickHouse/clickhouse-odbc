#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// HTTP(S) client of the driver, built on libcurl.
//
// HttpSession is created per ODBC connection; HttpResponse per request. The response body is
// read directly from HttpResponse (eof/get/read), which pulls data from the network lazily.
//
// ClickHouse may report an error after it has already started sending a successful response
// (a "mid-stream exception"): the exception text is appended to the body and the connection is
// closed without finishing the chunked encoding. To be able to find that text, HttpResponse
// holds back the last bytes of the body from the reader while the transfer is still running,
// and inspects them if the transfer fails.

struct HttpSettings {
    std::uint32_t connection_timeout = 30; // seconds, for establishing a connection
    std::uint32_t timeout = 30;            // seconds of inactivity while waiting for the server
    bool tls_verify = true;
    std::string ca_location;               // Linux/macOS: CA bundle file or directory
    std::string certificate_file;          // Linux/macOS: client certificate (PEM)
    std::string private_key_file;          // Linux/macOS: client key (PEM); Windows: client certificate, see below
};

/// A transport-level failure that happened before the response headers were received.
class HttpTransportError
    : public std::runtime_error
{
public:
    HttpTransportError(const std::string & message, bool retryable_)
        : std::runtime_error(message), retryable(retryable_) {}

    const bool retryable;
};

/// Per ODBC connection: settings, connection pool and TLS session cache.
/// Like the rest of a connection, it must only be used by one thread at a time; the curl share
/// therefore has no lock callbacks.
class HttpSession {
public:
    explicit HttpSession(HttpSettings settings);
    ~HttpSession();

    HttpSession(const HttpSession &) = delete;
    HttpSession & operator=(const HttpSession &) = delete;

    HttpSettings settings;

private:
    friend class HttpResponse;

    void * share = nullptr; // CURLSH

    // Windows: the certificate selected by `PrivateKeyFile`, as "CurrentUser\MY\<thumbprint>".
    std::string client_certificate;
};

struct HttpRequest {
    std::string url;
    std::shared_ptr<const std::string> body;
    std::vector<std::string> headers; // "Name: value"
};

/// A single POST request and its response. The constructor sends the request and blocks until
/// the response headers are received, throwing HttpTransportError if that fails. The body is then
/// read with eof(), get() and read(). Any failure while reading the body (transport error, broken
/// compressed data, ClickHouse exception) is thrown from these, and again from every later call.
class HttpResponse {
public:
    HttpResponse(std::shared_ptr<HttpSession> session, HttpRequest request);
    ~HttpResponse();

    HttpResponse(const HttpResponse &) = delete;
    HttpResponse & operator=(const HttpResponse &) = delete;

    long getStatus() const { return status; }
    bool hasHeader(const std::string & name) const;
    std::string getHeader(const std::string & name, const std::string & default_value = {}) const;

    /// The target of a redirect, resolved against the request URL. Empty if there is none.
    std::string getRedirectUrl() const;

    /// True if the body has no more data. Waits for data, if necessary.
    bool eof() {
        return (pos == end && !fill());
    }

    /// Reads one byte of the body; throws if there is none.
    char get() {
        if (pos == end && !fill())
            throw std::runtime_error("Incomplete input stream, expected at least 1 more byte");
        return *pos++;
    }

    /// Reads exactly `count` bytes of the body; throws if there are fewer.
    void read(char * dest, std::size_t count) {
        if (static_cast<std::size_t>(end - pos) >= count) { // the common case
            std::memcpy(dest, pos, count);
            pos += count;
            return;
        }
        readSlow(dest, count);
    }

    /// Reads the rest of the body (at most `limit` bytes). Never throws because of a failed transfer.
    std::string readAll(std::size_t limit);

private:
    /// Makes the next part of the body available in [pos, end). Returns false at the end of the
    /// body, throws if the body cannot be read. Must only be called when [pos, end) is empty: it
    /// may move or overwrite the data that the window pointed to.
    bool fill();
    void readSlow(char * dest, std::size_t count);

    void pump();
    void finish();
    std::string decodeRemaining();
    std::string transportErrorText() const;

    static std::size_t onHeader(char * data, std::size_t size, std::size_t count, void * self);
    static std::size_t onBody(char * data, std::size_t size, std::size_t count, void * self);

private:
    struct Handles;
    struct Zstd;

    std::shared_ptr<HttpSession> session;
    HttpRequest request;

    long status = 0;
    bool headers_complete = false;
    std::map<std::string, std::string> headers; // lower-case names

    bool done = false;
    int result = 0; // CURLcode of the finished transfer
    // The failure of the body, once there is one. fill() throws it as soon as it is detected, and
    // keeps it so that every later read throws the same exception again: retrying would otherwise
    // run into a decoder (and a tail that was scanned for a ClickHouse exception already) in an
    // undefined state, and might report another error, or even a clean end of a truncated body.
    std::exception_ptr error;
    std::string failed_tail; // undelivered (decoded) body bytes, if the transfer failed
    std::string unsupported_encoding;

    const char * pos = nullptr; // the part of the body that can be read now, without waiting
    const char * end = nullptr;

    std::string raw;         // received body bytes, possibly compressed
    std::size_t raw_pos = 0; // first byte of `raw` not yet handed over to the decoder/reader
    std::size_t raw_total = 0;
    std::unique_ptr<Zstd> zstd;
    std::vector<char> decoded;
    bool zstd_frame_complete = false;

    // Inactivity timeout: no data in either direction for `settings.timeout` seconds.
    std::size_t header_bytes = 0;
    std::int64_t activity = 0;
    std::chrono::steady_clock::time_point last_activity{};

    std::unique_ptr<Handles> handles; // last: destroyed first, before the buffers curl writes to
};
