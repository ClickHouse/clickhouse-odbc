#include <gtest/gtest.h>

#include "driver/statement.h"

namespace {

std::string unexpectedTimezoneFallback() {
    ADD_FAILURE() << "The local timezone fallback must not be evaluated when the response has a timezone header";
    return {};
}

std::string expectedTimezoneFallback() {
    return "local-timezone";
}

TEST(Statement, ResponseTimezoneUsesHeaderWithoutFallback) {
    Poco::Net::HTTPResponse response;
    response.set("X-ClickHouse-Timezone", "UTC");

    EXPECT_EQ(Statement::getResponseTimezone(response, unexpectedTimezoneFallback), "UTC");
}

TEST(Statement, ResponseTimezoneUsesEmptyHeaderWithoutFallback) {
    Poco::Net::HTTPResponse response;
    response.set("X-ClickHouse-Timezone", "");

    EXPECT_EQ(Statement::getResponseTimezone(response, unexpectedTimezoneFallback), "");
}

TEST(Statement, ResponseTimezoneUsesFallbackWithoutHeader) {
    Poco::Net::HTTPResponse response;

    EXPECT_EQ(Statement::getResponseTimezone(response, expectedTimezoneFallback), "local-timezone");
}

} // namespace
