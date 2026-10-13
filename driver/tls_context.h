#pragma once

#include <string>

#include <Poco/Net/Context.h>

// Builds the per-connection TLS client context used by Connection::connect()
// on the OpenSSL backend: the verification mode and the invalid-certificate
// handler follow this connection's own SSLMode, and CALocation /
// PrivateKeyFile / CertificateFile are loaded for this connection only.
// Throws Poco::Exception on construction (reported by the driver as an ODBC
// error) if the given files cannot be loaded.
Poco::Net::Context::Ptr makeClientContext(
    bool ssl_strict, const std::string & privateKeyFile, const std::string & certificateFile, const std::string & caLocation);
