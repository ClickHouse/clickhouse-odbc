#include "driver/tls_context.h"

#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/Net/Context.h>
#include <Poco/Net/RejectCertificateHandler.h>
#include <Poco/Exception.h>

#include <gtest/gtest.h>

// TLS settings must be applied per-connection: the context built for a
// connection has to reflect that connection's own SSLMode and files, and not
// whatever the first HTTPS connection in the process installed globally.

#if !defined(SECURITY_WIN32)

TEST(TlsContext, StrictModeCreatesStrictContext) {
    const auto ctx = makeClientContext(true, "", "", "");
    ASSERT_TRUE(ctx);
    EXPECT_EQ(Poco::Net::Context::VERIFY_STRICT, ctx->verificationMode());
}

TEST(TlsContext, AllowModeCreatesRelaxedContext) {
    const auto ctx = makeClientContext(false, "", "", "");
    ASSERT_TRUE(ctx);
    EXPECT_EQ(Poco::Net::Context::VERIFY_RELAXED, ctx->verificationMode());
}

TEST(TlsContext, StrictModeRejectsInvalidCertificates) {
    const auto ctx = makeClientContext(true, "", "", "");
    ASSERT_TRUE(ctx->getInvalidCertificateHandler());
    EXPECT_NE(nullptr, dynamic_cast<Poco::Net::RejectCertificateHandler *>(ctx->getInvalidCertificateHandler().get()));
}

TEST(TlsContext, AllowModeAcceptsInvalidCertificates) {
    const auto ctx = makeClientContext(false, "", "", "");
    ASSERT_TRUE(ctx->getInvalidCertificateHandler());
    EXPECT_NE(nullptr, dynamic_cast<Poco::Net::AcceptCertificateHandler *>(ctx->getInvalidCertificateHandler().get()));
}

TEST(TlsContext, LaterConnectionsDoNotInheritEarlierTlsSettings) {
    const auto allow_ctx = makeClientContext(false, "", "", "");
    const auto strict_ctx = makeClientContext(true, "", "", "");
    EXPECT_EQ(Poco::Net::Context::VERIFY_RELAXED, allow_ctx->verificationMode());
    EXPECT_EQ(Poco::Net::Context::VERIFY_STRICT, strict_ctx->verificationMode());
    EXPECT_NE(nullptr, dynamic_cast<Poco::Net::AcceptCertificateHandler *>(allow_ctx->getInvalidCertificateHandler().get()));
    EXPECT_NE(nullptr, dynamic_cast<Poco::Net::RejectCertificateHandler *>(strict_ctx->getInvalidCertificateHandler().get()));
}

TEST(TlsContext, MissingCALocationIsAReportableError) {
    // A CA file that cannot be loaded must surface as a regular exception
    // (reported by the driver as an ODBC error), never abort the process
    // through std::call_once unwinding.
    try {
        makeClientContext(true, "", "", "/nonexistent/clickhouse-odbc-test-ca.pem");
        FAIL() << "expected an exception for a CA file that cannot be loaded";
    }
    catch (const Poco::Exception &) {
    }
}

#endif // !defined(SECURITY_WIN32)
