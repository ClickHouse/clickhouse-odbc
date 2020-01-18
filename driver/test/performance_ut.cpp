#include "driver/platform/platform.h"
#include "driver/api/impl/impl.h"
#include "driver/driver.h"
#include "driver/environment.h"
#include "driver/connection.h"
#include "driver/statement.h"
#include "driver/test/common_utils.h"

#include <gtest/gtest.h>

#include <cstring>

class PerformanceTest
    : public ::testing::Test
{
protected:
    virtual void SetUp() override {
        driver_log = Driver::getInstance().getAttrAs<SQLUINTEGER>(CH_SQL_ATTR_DRIVERLOG, SQL_OPT_TRACE_ON);
        if (driver_log == SQL_OPT_TRACE_ON) {
            std::cout << "Temporarily disabling driver logging..." << std::endl;
            Driver::getInstance().setAttr(CH_SQL_ATTR_DRIVERLOG, SQL_OPT_TRACE_OFF);
        }
    }

    virtual void TearDown() override {
        if (driver_log == SQL_OPT_TRACE_ON) {
            std::cout << "Re-enabling driver logging..." << std::endl;
            Driver::getInstance().setAttr(CH_SQL_ATTR_DRIVERLOG, SQL_OPT_TRACE_ON);
        }
    }

private:
    SQLUINTEGER driver_log = SQL_OPT_TRACE_ON;
};

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(DispatchWith_CALL)) {
    constexpr std::size_t call_count = 100'000'000;

    START_MEASURING_TIME();

    for (std::size_t i = 0; i < call_count; ++i) {
        CALL([] () { return SQL_SUCCESS; });
    }

    STOP_MEASURING_TIME_AND_REPORT();
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(DispatchWith_CALL_WITH_HANDLE)) {
    constexpr std::size_t call_count = 100'000'000;

    SQLHENV henv = 0;
    SQLHDBC hdbc = 0;
    SQLHSTMT hstmt = 0;

    {
        const auto rc = impl::allocEnv(&henv);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        const auto rc = impl::allocConnect(henv, &hdbc);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        const auto rc = impl::allocStmt(hdbc, &hstmt);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    START_MEASURING_TIME();
    
    for (std::size_t i = 0; i < call_count; ++i) {
        CALL_WITH_HANDLE(hstmt, [] (Statement & statement) { return SQL_SUCCESS; });
    }

    STOP_MEASURING_TIME_AND_REPORT();
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(DispatchWith_CALL_WITH_HANDLE_SKIP_DIAG)) {
    constexpr std::size_t call_count = 100'000'000;

    SQLHENV henv = 0;
    SQLHDBC hdbc = 0;
    SQLHSTMT hstmt = 0;

    {
        const auto rc = impl::allocEnv(&henv);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        const auto rc = impl::allocConnect(henv, &hdbc);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        const auto rc = impl::allocStmt(hdbc, &hstmt);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    START_MEASURING_TIME();
    
    for (std::size_t i = 0; i < call_count; ++i) {
        CALL_WITH_HANDLE_SKIP_DIAG(hstmt, [] (Statement & statement) { return SQL_SUCCESS; });
    }

    STOP_MEASURING_TIME_AND_REPORT();
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(DispatchWith_CALL_WITH_TYPED_HANDLE)) {
    constexpr std::size_t call_count = 100'000'000;

    SQLHENV henv = 0;
    SQLHDBC hdbc = 0;
    SQLHSTMT hstmt = 0;

    {
        const auto rc = impl::allocEnv(&henv);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        const auto rc = impl::allocConnect(henv, &hdbc);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        const auto rc = impl::allocStmt(hdbc, &hstmt);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    START_MEASURING_TIME();
    
    for (std::size_t i = 0; i < call_count; ++i) {
        CALL_WITH_TYPED_HANDLE(SQL_HANDLE_STMT, hstmt, [] (Statement & statement) { return SQL_SUCCESS; });
    }

    STOP_MEASURING_TIME_AND_REPORT();
}

TEST_F(PerformanceTest, ENABLE_FOR_OPTIMIZED_BUILDS_ONLY(DispatchWith_CALL_WITH_TYPED_HANDLE_SKIP_DIAG)) {
    constexpr std::size_t call_count = 100'000'000;

    SQLHENV henv = 0;
    SQLHDBC hdbc = 0;
    SQLHSTMT hstmt = 0;

    {
        const auto rc = impl::allocEnv(&henv);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        const auto rc = impl::allocConnect(henv, &hdbc);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    {
        const auto rc = impl::allocStmt(hdbc, &hstmt);
        ASSERT_EQ(rc, SQL_SUCCESS);
    }

    START_MEASURING_TIME();
    
    for (std::size_t i = 0; i < call_count; ++i) {
        CALL_WITH_TYPED_HANDLE_SKIP_DIAG(SQL_HANDLE_STMT, hstmt, [] (Statement & statement) { return SQL_SUCCESS; });
    }

    STOP_MEASURING_TIME_AND_REPORT();
}
