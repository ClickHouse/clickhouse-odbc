#pragma once

#include <chrono>
#include <cstdint>
#include <exception>
#include <istream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <vector>

// HTTP(S) client of the driver, built on libcurl.
//
// HttpSession is created per ODBC connection; HttpResponse per request. The response body is
// read through HttpResponse, which is a std::istream that pulls data from the network lazily.
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
/// Must not be used by several threads at the same time.
class HttpSession {
public:
    explicit HttpSession(HttpSettings settings);
    ~HttpSession();

    HttpSession(const HttpSession &) = delete;
    HttpSession & operator=(const HttpSession &) = delete;

    HttpSettings settings;

private:
    friend class HttpResponse;

    static void lock(void * handle, int data, int access, void * self);
    static void unlock(void * handle, int data, void * self);

    void * share = nullptr; // CURLSH
    std::mutex locks[16];

    // Windows: the certificate selected by `PrivateKeyFile`, as "CurrentUser\MY\<thumbprint>".
    std::string client_certificate;
};

struct HttpRequest {
    std::string url;
    std::shared_ptr<const std::string> body;
    std::vector<std::string> headers; // "Name: value"
};

/// A single POST request and its response. The constructor sends the request and blocks until
/// the response headers are received, throwing HttpTransportError if that fails. The body is
/// then read through the std::istream interface or directly through rdbuf(). Any failure while
/// reading the body (transport error, broken compressed data, ClickHouse exception) is thrown from
/// the read operation itself, and again from every following read. (The stream has std::ios::badbit
/// set in exceptions(), so std::istream operations rethrow it too, but only once.)
class HttpResponse
    : private std::streambuf
    , public std::istream
{
public:
    HttpResponse(std::shared_ptr<HttpSession> session, HttpRequest request);
    ~HttpResponse() override;

    long getStatus() const { return status; }
    bool hasHeader(const std::string & name) const;
    std::string getHeader(const std::string & name, const std::string & default_value = {}) const;

    /// The target of a redirect, resolved against the request URL. Empty if there is none.
    std::string getRedirectUrl() const;

    /// Reads the rest of the body (at most `limit` bytes). Never throws because of a failed transfer.
    std::string readAll(std::size_t limit);

private:
    // std::streambuf, std::istream and std::ios all define these.
    using traits_type = std::streambuf::traits_type;
    using int_type = std::streambuf::int_type;

    int_type underflow() override;

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
    std::exception_ptr error;
    std::string failed_tail; // undelivered (decoded) body bytes, if the transfer failed
    std::string unsupported_encoding;

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
