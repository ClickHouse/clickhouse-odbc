#include "driver/platform/platform.h"
#include "driver/test/client_test_base.h"
#include "driver/test/client_utils.h"

#include <array>
#include <clocale>
#include <string>
#include <tuple>

namespace {

class RestoreNumericLocale {
public:
    ~RestoreNumericLocale() { std::setlocale(LC_NUMERIC, previous.c_str()); }

private:
    const std::string previous = std::setlocale(LC_NUMERIC, nullptr);
};

class FloatingPointFetchTest : public ClientTestWithParamBase<std::tuple<bool, bool, SQLSMALLINT>> {
};

TEST_P(FloatingPointFetchTest, NumericLocale)
{
    const auto [comma, bind, c_type] = GetParam();
    RestoreNumericLocale restore;
    if (comma) {
#if defined(_win_)
        const auto * locale = std::setlocale(LC_NUMERIC, "de-DE");
#else
        const auto * locale = std::setlocale(LC_NUMERIC, "de_DE.UTF-8");
#endif
        if (!locale)
            GTEST_SKIP() << "No German LC_NUMERIC locale is installed";
        ASSERT_STREQ(std::localeconv()->decimal_point, ",");
    }
    else {
        ASSERT_NE(std::setlocale(LC_NUMERIC, "C"), nullptr);
        ASSERT_STREQ(std::localeconv()->decimal_point, ".");
    }

    auto query = fromUTF8<PTChar>("SELECT toFloat64(0.125), toFloat32(-2.5), "
        "CAST(0.25 AS Decimal(18, 8)), toFloat64(42), toFloat64(1e-7)");
    STMT_OK(SQLExecDirect(hstmt, ptcharCast(query.data()), SQL_NTS));

    auto verify = [&]<typename T>(const std::array<T, 5> & expected) {
        std::array<T, 5> actual{};
        std::array<SQLLEN, 5> lengths{};
        if (bind) {
            for (SQLUSMALLINT i = 0; i < actual.size(); ++i)
                STMT_OK(SQLBindCol(hstmt, i + 1, c_type, &actual[i], sizeof(T), &lengths[i]));
        }

        STMT_OK(SQLFetch(hstmt));
        if (!bind) {
            for (SQLUSMALLINT i = 0; i < actual.size(); ++i)
                STMT_OK(SQLGetData(hstmt, i + 1, c_type, &actual[i], sizeof(T), &lengths[i]));
        }

        for (SQLUSMALLINT i = 0; i < actual.size(); ++i) {
            EXPECT_EQ(actual[i], expected[i]) << "column " << i + 1;
            EXPECT_EQ(lengths[i], sizeof(T)) << "column " << i + 1;
        }

        STMT_OK(SQLFreeStmt(hstmt, SQL_UNBIND));
        STMT_OK(SQLFreeStmt(hstmt, SQL_CLOSE));
    };

    if (c_type == SQL_C_FLOAT)
        verify(std::array{0.125f, -2.5f, 0.25f, 42.0f, 1e-7f});
    else
        verify(std::array{0.125, -2.5, 0.25, 42.0, 1e-7});

    EXPECT_STREQ(std::localeconv()->decimal_point, comma ? "," : ".");
}

INSTANTIATE_TEST_SUITE_P(
    FloatAndDouble,
    FloatingPointFetchTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool(), ::testing::Values(SQL_C_FLOAT, SQL_C_DOUBLE)),
    ([](const auto & info) {
        const auto [comma, bind, type] = info.param;
        return std::string(comma ? "Comma" : "Classic") + (bind ? "BindCol" : "GetData") + (type == SQL_C_FLOAT ? "Float" : "Double");
    }));

}
