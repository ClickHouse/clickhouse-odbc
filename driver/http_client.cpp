#include "driver/http_client.h"
#include "driver/exception.h"

#include <curl/curl.h>
#include <zstd.h>

#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string_view>

#if defined(_WIN32)
#    include <windows.h>
#    include <wincrypt.h>
#endif

namespace {

// ClickHouse mid-stream exceptions are assumed to be shorter than this (the server truncates
// the message to about 16 KiB). While the transfer is running, this many trailing body bytes are
// held back from the reader, so that the exception text is still available if the transfer fails.
constexpr std::size_t HOLDBACK_SIZE = 32 * 1024;

constexpr char EXCEPTION_TAG_HEADER[] = "x-clickhouse-exception-tag";

std::once_flag curl_global_init_flag;

void ensureCurlGlobalInit() {
    // Never called from DllMain. curl_global_cleanup() is intentionally never called: the driver
    // cannot know when the last user is gone, and the process-wide state is small.
    std::call_once(curl_global_init_flag, [] {
        const auto rc = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (rc != CURLE_OK)
            throw std::runtime_error(std::string("curl_global_init() failed: ") + curl_easy_strerror(rc));
    });
}

std::string toLower(std::string_view s) { // ASCII only, independent of the locale
    std::string res(s);
    for (auto & c : res) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return res;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
        s.remove_suffix(1);
    return s;
}

bool isRetryable(CURLcode code) {
    switch (code) {
        case CURLE_UNSUPPORTED_PROTOCOL:
        case CURLE_URL_MALFORMAT:
        case CURLE_OUT_OF_MEMORY:
        case CURLE_BAD_FUNCTION_ARGUMENT:
        case CURLE_PEER_FAILED_VERIFICATION:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case CURLE_SSL_CACERT_BADFILE:
        case CURLE_SSL_ISSUER_ERROR:
        case CURLE_SSL_CRL_BADFILE:
        case CURLE_WRITE_ERROR:
        case CURLE_ABORTED_BY_CALLBACK:
        // Timeouts are not retried: the server may still be executing the query.
        case CURLE_OPERATION_TIMEDOUT:
            return false;
        default:
            return true;
    }
}

/// Finds the message of a ClickHouse mid-stream exception at the end of the body.
std::optional<std::string> findClickHouseExceptionMessage(const std::string & exception_tag, std::string_view buffer) {
    if (!exception_tag.empty()) {
        // The format, when the server sends X-ClickHouse-Exception-Tag:
        // ...__exception__\r\n<tag>\r\n<message><message size> <tag>\r\n__exception__\r\n
        const std::string closing_marker = " " + exception_tag + "\r\n__exception__\r\n";
        if (!buffer.ends_with(closing_marker))
            return std::nullopt;

        const char * begin = buffer.data();
        const char * end = buffer.data() + buffer.size() - closing_marker.size();
        const char * size_begin = end;
        while (size_begin > begin && std::isdigit(static_cast<unsigned char>(size_begin[-1])))
            --size_begin;
        if (size_begin == end)
            return std::nullopt;

        std::size_t message_size = 0;
        if (std::from_chars(size_begin, end, message_size).ec != std::errc())
            return std::nullopt;
        if (static_cast<std::size_t>(size_begin - begin) < message_size)
            return std::nullopt;
        return std::string(size_begin - message_size, size_begin);
    }

    // Older servers: everything after the last marker is the message. This may be a false
    // positive if the data itself contains the marker.
    static constexpr std::string_view marker{"__exception__\r\n"};
    const auto pos = buffer.rfind(marker);
    if (pos != std::string_view::npos)
        return std::string(buffer.substr(pos + marker.size()));
    return std::nullopt;
}

/// "Code: 395. DB::Exception: ..." -> "395"
std::string parseExceptionCode(std::string_view message) {
    const auto pos = message.find("Code: ");
    if (pos == std::string_view::npos)
        return {};
    const auto begin = pos + 6;
    auto end = begin;
    while (end < message.size() && std::isdigit(static_cast<unsigned char>(message[end])))
        ++end;
    return std::string(message.substr(begin, end - begin));
}

#if defined(_WIN32)

/// Translates the `PrivateKeyFile` value into a certificate reference understood by curl/Schannel.
/// As before (Poco), the value is the subject common name of a certificate in CurrentUser\MY.
/// Values containing a backslash, e.g. "CurrentUser\MY\<thumbprint>", are passed to curl as is.
std::string windowsClientCertificate(const std::string & name) {
    if (name.ends_with(".pfx") || name.ends_with(".p12") || name.ends_with(".PFX") || name.ends_with(".P12")) {
        throw std::runtime_error(
            "PKCS#12 client certificate files are not supported on Windows: import the certificate into "
            "the CurrentUser\\MY certificate store and set PrivateKeyFile to its subject common name");
    }

    if (name.find('\\') != std::string::npos)
        return name;

    HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
        CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_READONLY_FLAG | CERT_STORE_OPEN_EXISTING_FLAG, L"MY");
    if (!store)
        throw std::runtime_error("Cannot open certificate store CurrentUser\\MY");

    // Compare the decoded common name, so that the match does not depend on the ASN.1 string type
    // of the CN (UTF8String, PrintableString, BMPString).
    const int wide_size = MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), nullptr, 0);
    std::wstring wide_name(static_cast<std::size_t>(wide_size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), wide_name.data(), wide_size);

    PCCERT_CONTEXT cert = nullptr;
    while ((cert = CertEnumCertificatesInStore(store, cert)) != nullptr) {
        wchar_t cn[512] = {};
        CertGetNameStringW(cert, CERT_NAME_ATTR_TYPE, 0, const_cast<char *>(szOID_COMMON_NAME), cn, 512);
        if (wide_name == cn)
            break; // keep `cert`, the next call of CertEnumCertificatesInStore() would free it
    }

    BYTE hash[20];
    DWORD hash_size = sizeof(hash);
    const bool found = cert && CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, hash, &hash_size);
    if (cert)
        CertFreeCertificateContext(cert);
    CertCloseStore(store, 0);

    if (!found)
        throw std::runtime_error("Failed to find certificate " + name + " in store CurrentUser\\MY");

    static constexpr char digits[] = "0123456789ABCDEF";
    std::string res = "CurrentUser\\MY\\";
    for (DWORD i = 0; i < hash_size; ++i) {
        res += digits[hash[i] >> 4];
        res += digits[hash[i] & 0xF];
    }
    return res;
}

#endif

} // namespace

// ------------------------------------------------------------------------------------------------
// HttpSession
// ------------------------------------------------------------------------------------------------

HttpSession::HttpSession(HttpSettings settings_)
    : settings(std::move(settings_))
{
#if defined(_WIN32)
    if (!settings.private_key_file.empty())
        client_certificate = windowsClientCertificate(settings.private_key_file);
#endif

    ensureCurlGlobalInit();

    share = curl_share_init();
    if (!share)
        throw std::runtime_error("curl_share_init() failed");

    // DNS results are not shared and not cached (see CURLOPT_DNS_CACHE_TIMEOUT): every new
    // connection resolves the host name again, which matters when the server address changes.
    if (curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT) != CURLSHE_OK ||
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION) != CURLSHE_OK)
    {
        curl_share_cleanup(share);
        throw std::runtime_error("curl_share_setopt() failed");
    }
}

HttpSession::~HttpSession() {
    // Every HttpResponse holds a reference to its session, so no transfer uses the share here.
    curl_share_cleanup(share);
}

// ------------------------------------------------------------------------------------------------
// HttpResponse
// ------------------------------------------------------------------------------------------------

struct HttpResponse::Handles {
    CURLM * multi = nullptr;
    CURL * easy = nullptr;
    curl_slist * headers = nullptr;
    char error_buffer[CURL_ERROR_SIZE] = {};

    ~Handles() {
        if (multi && easy)
            curl_multi_remove_handle(multi, easy); // aborts an unfinished transfer and closes its connection
        if (easy)
            curl_easy_cleanup(easy);
        if (multi)
            curl_multi_cleanup(multi);
        if (headers)
            curl_slist_free_all(headers);
    }
};

struct HttpResponse::Zstd {
    ZSTD_DStream * stream = ZSTD_createDStream();

    Zstd() {
        if (!stream)
            throw std::bad_alloc();
    }

    ~Zstd() {
        ZSTD_freeDStream(stream);
    }
};

HttpResponse::HttpResponse(std::shared_ptr<HttpSession> session_, HttpRequest request_)
    : std::istream(static_cast<std::streambuf *>(this))
    , session(std::move(session_))
    , request(std::move(request_))
    , handles(std::make_unique<Handles>())
{
    exceptions(std::ios::badbit); // failures thrown from underflow() reach the reader

    const auto & settings = session->settings;

    handles->multi = curl_multi_init();
    handles->easy = curl_easy_init();
    if (!handles->multi || !handles->easy)
        throw std::runtime_error("Failed to initialize libcurl handles");

    for (const auto & header : request.headers)
        handles->headers = curl_slist_append(handles->headers, header.c_str());

    // No implicit Content-Type/Accept headers, and do not wait for "100 Continue".
    handles->headers = curl_slist_append(handles->headers, "Expect:");
    handles->headers = curl_slist_append(handles->headers, "Content-Type:");
    handles->headers = curl_slist_append(handles->headers, "Accept:");

    CURL * easy = handles->easy;

    curl_easy_setopt(easy, CURLOPT_SHARE, session->share);
    curl_easy_setopt(easy, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(easy, CURLOPT_POST, 1L);
    curl_easy_setopt(easy, CURLOPT_POSTFIELDS, request.body->data());
    curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request.body->size()));
    curl_easy_setopt(easy, CURLOPT_HTTPHEADER, handles->headers);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);       // redirects are handled by the caller
    curl_easy_setopt(easy, CURLOPT_HTTP_CONTENT_DECODING, 0L); // zstd is decoded here
    curl_easy_setopt(easy, CURLOPT_PROXY, "");                 // ignore the *_proxy environment variables
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_DNS_CACHE_TIMEOUT, 0L);
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, handles->error_buffer);
    curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, &HttpResponse::onHeader);
    curl_easy_setopt(easy, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, &HttpResponse::onBody);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, this);

    // The connect phase is limited by CURLOPT_CONNECTTIMEOUT; after that `settings.timeout` is an
    // inactivity timeout, implemented in pump() (CURLOPT_LOW_SPEED_* averages over a time window,
    // which is not the same).
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, static_cast<long>(settings.connection_timeout));

    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, settings.tls_verify ? 1L : 0L);
    curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, settings.tls_verify ? 2L : 0L);

#if defined(_WIN32)
    // Fail only if a certificate is actually revoked, not if the revocation status is unavailable.
    curl_easy_setopt(easy, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_REVOKE_BEST_EFFORT));
    if (!session->client_certificate.empty())
        curl_easy_setopt(easy, CURLOPT_SSLCERT, session->client_certificate.c_str());
#else
    if (!settings.ca_location.empty()) {
        std::error_code ec;
        if (std::filesystem::is_directory(settings.ca_location, ec))
            curl_easy_setopt(easy, CURLOPT_CAPATH, settings.ca_location.c_str());
        else
            curl_easy_setopt(easy, CURLOPT_CAINFO, settings.ca_location.c_str());
    }
    if (!settings.certificate_file.empty())
        curl_easy_setopt(easy, CURLOPT_SSLCERT, settings.certificate_file.c_str());
    if (!settings.private_key_file.empty())
        curl_easy_setopt(easy, CURLOPT_SSLKEY, settings.private_key_file.c_str());
    curl_easy_setopt(easy, CURLOPT_KEYPASSWD, ""); // never prompt on the terminal for a key passphrase
#endif

    if (curl_multi_add_handle(handles->multi, easy) != CURLM_OK)
        throw std::runtime_error("curl_multi_add_handle() failed");

    last_activity = std::chrono::steady_clock::now();
    while (!headers_complete && !done)
        pump();

    if (!headers_complete) {
        // Once the server has started responding, it may be executing the query already:
        // then retrying could execute it twice.
        const bool retryable = (header_bytes == 0 && isRetryable(static_cast<CURLcode>(result)));
        throw HttpTransportError(transportErrorText(), retryable);
    }

    const auto encoding = toLower(getHeader("Content-Encoding"));
    if (encoding == "zstd") {
        zstd = std::make_unique<Zstd>();
        decoded.resize(ZSTD_DStreamOutSize());
    }
    else if (!encoding.empty() && encoding != "identity" && encoding != "none") {
        unsupported_encoding = encoding; // reported when the body is read, the status is still useful
    }
}

HttpResponse::~HttpResponse() {
    handles.reset(); // detach from curl before the buffers its callbacks write to are destroyed
}

bool HttpResponse::hasHeader(const std::string & name) const {
    return headers.find(toLower(name)) != headers.end();
}

std::string HttpResponse::getHeader(const std::string & name, const std::string & default_value) const {
    const auto it = headers.find(toLower(name));
    return (it == headers.end() ? default_value : it->second);
}

std::string HttpResponse::getRedirectUrl() const {
    char * url = nullptr;
    curl_easy_getinfo(handles->easy, CURLINFO_REDIRECT_URL, &url);
    return (url ? std::string(url) : std::string());
}

std::string HttpResponse::readAll(std::size_t limit) {
    std::string res;
    try {
        char buffer[4096];
        while (res.size() < limit) {
            const auto n = sgetn(buffer, sizeof(buffer));
            if (n <= 0)
                break;
            res.append(buffer, static_cast<std::size_t>(n));
        }
    }
    catch (...) {
        res += failed_tail; // whatever was received before the transfer failed
    }
    if (res.size() > limit)
        res.resize(limit);
    return res;
}

std::string HttpResponse::transportErrorText() const {
    if (handles->error_buffer[0])
        return handles->error_buffer;
    return curl_easy_strerror(static_cast<CURLcode>(result));
}

std::size_t HttpResponse::onHeader(char * data, std::size_t size, std::size_t count, void * self_ptr) {
    auto & self = *static_cast<HttpResponse *>(self_ptr);
    const std::string_view line(data, size * count);
    self.header_bytes += line.size(); // e.g. X-ClickHouse-Progress headers keep the request alive

    try {
        if (line.starts_with("HTTP/")) { // a new, possibly interim (1xx), response
            self.headers.clear();
            self.headers_complete = false;
        }
        else if (trim(line).empty()) {
            long code = 0;
            curl_easy_getinfo(self.handles->easy, CURLINFO_RESPONSE_CODE, &code);
            if (code >= 200) {
                self.status = code;
                self.headers_complete = true;
            }
        }
        else if (const auto colon = line.find(':'); colon != std::string_view::npos && !self.headers_complete) {
            self.headers[toLower(trim(line.substr(0, colon)))] = std::string(trim(line.substr(colon + 1)));
        }
    }
    catch (...) {
        return 0; // out of memory: abort the transfer, never throw through curl
    }

    return size * count;
}

std::size_t HttpResponse::onBody(char * data, std::size_t size, std::size_t count, void * self_ptr) {
    auto & self = *static_cast<HttpResponse *>(self_ptr);
    try {
        self.raw.append(data, size * count);
    }
    catch (...) {
        return 0; // out of memory: abort the transfer, never throw through curl
    }
    self.raw_total += size * count;
    return size * count;
}

void HttpResponse::pump() {
    const auto raw_before = raw.size();
    const auto headers_before = headers_complete;

    int running = 0;
    const auto mc = curl_multi_perform(handles->multi, &running);
    if (mc != CURLM_OK) {
        done = true;
        result = CURLE_RECV_ERROR;
        std::snprintf(handles->error_buffer, sizeof(handles->error_buffer), "%s", curl_multi_strerror(mc));
        return;
    }

    int queued = 0;
    while (CURLMsg * msg = curl_multi_info_read(handles->multi, &queued)) {
        if (msg->msg == CURLMSG_DONE && msg->easy_handle == handles->easy) {
            done = true;
            result = msg->data.result;
        }
    }

    if (done)
        return;

    // Inactivity timeout. Any progress (request upload, response headers or body) restarts it.
    // Until the connection is established, only CURLOPT_CONNECTTIMEOUT applies.
    curl_off_t uploaded = 0;
    curl_off_t pretransfer = 0;
    curl_easy_getinfo(handles->easy, CURLINFO_SIZE_UPLOAD_T, &uploaded);
    curl_easy_getinfo(handles->easy, CURLINFO_PRETRANSFER_TIME_T, &pretransfer);
    const auto new_activity = static_cast<std::int64_t>(raw_total + header_bytes + uploaded);
    const auto now = std::chrono::steady_clock::now();
    if (new_activity != activity || pretransfer == 0) {
        activity = new_activity;
        last_activity = now;
    }
    else if (now - last_activity >= std::chrono::seconds(session->settings.timeout)) {
        done = true;
        result = CURLE_OPERATION_TIMEDOUT;
        std::snprintf(handles->error_buffer, sizeof(handles->error_buffer), "Timeout");
        return;
    }

    if (raw.size() == raw_before && headers_complete == headers_before)
        curl_multi_poll(handles->multi, nullptr, 0, 200, nullptr);
}

HttpResponse::int_type HttpResponse::underflow() {
    if (gptr() < egptr())
        return traits_type::to_int_type(*gptr());

    setg(nullptr, nullptr, nullptr);

    if (!error && !unsupported_encoding.empty())
        error = std::make_exception_ptr(std::runtime_error("Unsupported Content-Encoding: " + unsupported_encoding));

    if (error)
        std::rethrow_exception(error);

    // Everything handed out so far has been consumed: compact the buffer.
    if (raw_pos > 0) {
        raw.erase(0, raw_pos);
        raw_pos = 0;
    }

    // Only the time spent waiting here counts towards the inactivity timeout, not the time the
    // application spends between reads.
    last_activity = std::chrono::steady_clock::now();

    for (;;) {
        const auto available = raw.size() - raw_pos;
        const auto allowed = (done ? available : (available > HOLDBACK_SIZE ? available - HOLDBACK_SIZE : 0));

        if (!done || result == CURLE_OK) {
            if (!zstd) {
                if (allowed > 0) {
                    char * begin = raw.data() + raw_pos;
                    setg(begin, begin, begin + allowed);
                    raw_pos += allowed;
                    return traits_type::to_int_type(*gptr());
                }
            }
            else if (allowed > 0 || done) {
                ZSTD_inBuffer in{raw.data() + raw_pos, allowed, 0};
                ZSTD_outBuffer out{decoded.data(), decoded.size(), 0};
                const auto rc = ZSTD_decompressStream(zstd->stream, &out, &in);
                if (ZSTD_isError(rc)) {
                    error = std::make_exception_ptr(
                        std::runtime_error(std::string("Failed to decompress the data: ") + ZSTD_getErrorName(rc)));
                    std::rethrow_exception(error);
                }
                raw_pos += in.pos;
                if (in.pos > 0 || out.pos > 0) // a call without progress returns a size hint, not 0
                    zstd_frame_complete = (rc == 0);
                if (out.pos > 0) {
                    setg(decoded.data(), decoded.data(), decoded.data() + out.pos);
                    return traits_type::to_int_type(*gptr());
                }
                if (in.pos > 0)
                    continue; // input consumed without output yet
            }
        }

        if (done) {
            finish();
            if (error)
                std::rethrow_exception(error);
            return traits_type::eof();
        }

        pump();
    }
}

void HttpResponse::finish() {
    if (result != CURLE_OK) {
        failed_tail = decodeRemaining();
        if (auto message = findClickHouseExceptionMessage(getHeader(EXCEPTION_TAG_HEADER), failed_tail))
            error = std::make_exception_ptr(ClickHouseException("ClickHouse exception: " + *message, parseExceptionCode(*message)));
        else
            error = std::make_exception_ptr(std::runtime_error(transportErrorText()));
        return;
    }

    if (zstd && raw_total > 0 && !zstd_frame_complete) {
        error = std::make_exception_ptr(std::runtime_error(
            "Failed to decompress the data: Incomplete data, frame was truncated or connection closed prematurely"));
    }
}

std::string HttpResponse::decodeRemaining() {
    if (!zstd)
        return raw.substr(raw_pos);

    // Decode as much as possible; a broken or truncated stream is expected here.
    std::string res;
    ZSTD_inBuffer in{raw.data() + raw_pos, raw.size() - raw_pos, 0};
    for (int stalled = 0; stalled < 100;) {
        ZSTD_outBuffer out{decoded.data(), decoded.size(), 0};
        const auto rc = ZSTD_decompressStream(zstd->stream, &out, &in);
        if (ZSTD_isError(rc))
            break;
        res.append(decoded.data(), out.pos);
        if (in.pos == in.size && out.pos < out.size)
            break;
        stalled = (out.pos == 0 ? stalled + 1 : 0);
    }
    return res;
}
