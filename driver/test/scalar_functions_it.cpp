#include <gtest/gtest.h>
#include "driver/test/client_test_base.h"

class ScalarFunctionsTest
    : public ClientTestBase
{
protected:
    void prepare(const std::string & query)
    {
        auto query_encoded = fromUTF8<PTChar>(query);
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query_encoded.data()), SQL_NTS));
    }

    template <typename SqlType>
    std::optional<SqlType> getData()
    {
        SqlType buffer{};
        SQLLEN indicator;
        ODBC_CALL_ON_STMT_THROW(
            hstmt, SQLGetData( hstmt, 1, getCTypeFor<SqlType>(), &buffer, sizeof(SqlType), &indicator));
        if (indicator == SQL_NULL_DATA) {
            return std::nullopt;
        }
        return buffer;
    }

    template <>
    std::optional<std::string> getData()
    {
        static const size_t max_string_size = 1024;
        std::string buffer(max_string_size, '\0');
        SQLLEN indicator;
        ODBC_CALL_ON_STMT_THROW(
            hstmt, SQLGetData( hstmt, 1, SQL_C_CHAR, buffer.data(), buffer.size(), &indicator));
        if (indicator == SQL_NULL_DATA) {
            return std::nullopt;
        }
        assert(indicator >= 0 && "cannot read size from a negative indicator");
        buffer.resize(indicator);
        return buffer;
    }

    template <typename SqlType>
    std::optional<SqlType> query(const std::string & query)
    {
        prepare(query);
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));
        auto res = getData<SqlType>();
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));
        return res;
    }
};

TEST_F(ScalarFunctionsTest, ASCII) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn ASCII('A')}"), 'A');
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn ASCII('abc')}"), 'a');  // Returns ASCII of first character 'a'
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn ASCII('0')}"), '0');
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn ASCII(' ')}"), ' ');
}

TEST_F(ScalarFunctionsTest, BIT_LENGTH) {
    // ASCII: 12 chars * 1 byte * 8 bits = 96 bits
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn BIT_LENGTH('Hello World!')}"), 96);
}

TEST_F(ScalarFunctionsTest, CHAR) {
    ASSERT_EQ(query<std::string>("SELECT {fn CHAR(65)}"), "A");
    ASSERT_EQ(query<std::string>("SELECT {fn CHAR(97)}"), "a");
    ASSERT_EQ(query<std::string>("SELECT {fn CHAR(48)}"), "0");
    ASSERT_EQ(query<std::string>("SELECT {fn CHAR(32)}"), " ");
}

TEST_F(ScalarFunctionsTest, CHAR_LENGTH) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn CHAR_LENGTH('Hello World!')}"), 12);
}

TEST_F(ScalarFunctionsTest, CHARACTER_LENGTH) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn CHARACTER_LENGTH('Hello World!')}"), 12);
}

TEST_F(ScalarFunctionsTest, CONCAT) {
    ASSERT_EQ(query<std::string>("SELECT {fn CONCAT('Hello', ' World')}"), "Hello World");
    ASSERT_EQ(query<std::string>("SELECT {fn CONCAT('foo', 'bar')}"), "foobar");
    ASSERT_EQ(query<std::string>("SELECT {fn CONCAT('', 'test')}"), "test");
}

TEST_F(ScalarFunctionsTest, DIFFERENCE) {
    // Identical soundex codes (S530) - maximum similarity
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DIFFERENCE('Smith', 'Smythe')}"), 4);
    // Same word - identical
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DIFFERENCE('Hello', 'Hello')}"), 4);
    // Similar sounding names
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DIFFERENCE('Green', 'Greene')}"), 4);
    // Different words - low similarity
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DIFFERENCE('Hello', 'Goodbye')}"), 1);
}

TEST_F(ScalarFunctionsTest, INSERT) {
    // INSERT(str1, start, length, str2) - insert str2 into str1 at start, replacing length chars
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('Hello World', 7, 5, 'There')}"), "Hello There");
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('abcdef', 3, 2, 'XYZ')}"), "abXYZef");

    // Insert at beginning (start=1, length=0 means pure insert)
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('World', 1, 0, 'Hello ')}"), "Hello World");

    // Edge case: start=0 (invalid position, returns NULL)
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('Hello', 0, 2, 'XYZ')}"), std::nullopt);

    // Edge case: start beyond string length
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('Hello', 10, 2, 'XYZ')}"), "HelloXYZ");

    // Edge case: length extends beyond string end (delete to end, then insert)
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('Hello', 3, 100, 'XYZ')}"), "HeXYZ");

    // Edge case: empty str2 (just delete)
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('Hello', 2, 3, '')}"), "Ho");

    // Edge case: empty str1
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('', 1, 0, 'Hello')}"), "Hello");
}

TEST_F(ScalarFunctionsTest, LCASE) {
    ASSERT_EQ(query<std::string>("SELECT {fn LCASE('Hello World')}"), "hello world");
    ASSERT_EQ(query<std::string>("SELECT {fn LCASE('UPPERCASE')}"), "uppercase");
    ASSERT_EQ(query<std::string>("SELECT {fn LCASE('MiXeD CaSe')}"), "mixed case");
}

TEST_F(ScalarFunctionsTest, LEFT) {
    ASSERT_EQ(query<std::string>("SELECT {fn LEFT('Hello World', 5)}"), "Hello");
    ASSERT_EQ(query<std::string>("SELECT {fn LEFT('Hello', 3)}"), "Hel");
    ASSERT_EQ(query<std::string>("SELECT {fn LEFT('Hi', 10)}"), "Hi");
}

TEST_F(ScalarFunctionsTest, LENGTH) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LENGTH('Hello World!')}"), 12);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LENGTH('')}"), 0);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LENGTH(NULL)}"), std::nullopt);
}

TEST_F(ScalarFunctionsTest, LOCATE) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LOCATE('World', 'Hello World')}"), 7);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LOCATE('o', 'Hello World')}"), 5);
    // With start position - find second 'o'
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LOCATE('o', 'Hello World', 6)}"), 8);
    // Not found
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LOCATE('xyz', 'Hello World')}"), 0);
}

TEST_F(ScalarFunctionsTest, LTRIM) {
    ASSERT_EQ(query<std::string>("SELECT {fn LTRIM('   Hello')}"), "Hello");
    ASSERT_EQ(query<std::string>("SELECT {fn LTRIM('  Hello World  ')}"), "Hello World  ");
    ASSERT_EQ(query<std::string>("SELECT {fn LTRIM('NoSpaces')}"), "NoSpaces");
}

TEST_F(ScalarFunctionsTest, OCTET_LENGTH) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn OCTET_LENGTH('Hello')}"), 5);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn OCTET_LENGTH('')}"), 0);
}

TEST_F(ScalarFunctionsTest, POSITION) {
    // POSITION(substr IN str) - same as LOCATE but different syntax
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn POSITION('World' IN 'Hello World')}"), 7);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn POSITION('o' IN 'Hello')}"), 5);
    // Not found
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn POSITION('xyz' IN 'Hello')}"), 0);
}

TEST_F(ScalarFunctionsTest, REPEAT) {
    ASSERT_EQ(query<std::string>("SELECT {fn REPEAT('ab', 3)}"), "ababab");
    ASSERT_EQ(query<std::string>("SELECT {fn REPEAT('Hello', 2)}"), "HelloHello");
    ASSERT_EQ(query<std::string>("SELECT {fn REPEAT('x', 5)}"), "xxxxx");
    // Zero repeats
    ASSERT_EQ(query<std::string>("SELECT {fn REPEAT('Hello', 0)}"), "");
}

TEST_F(ScalarFunctionsTest, REPLACE) {
    ASSERT_EQ(query<std::string>("SELECT {fn REPLACE('Hello World', 'World', 'There')}"), "Hello There");
    // Replace all occurrences
    ASSERT_EQ(query<std::string>("SELECT {fn REPLACE('ababa', 'a', 'x')}"), "xbxbx");
    // No match
    ASSERT_EQ(query<std::string>("SELECT {fn REPLACE('Hello', 'xyz', 'abc')}"), "Hello");
    // Replace with empty string (delete)
    ASSERT_EQ(query<std::string>("SELECT {fn REPLACE('Hello World', ' World', '')}"), "Hello");
}

TEST_F(ScalarFunctionsTest, RIGHT) {
    ASSERT_EQ(query<std::string>("SELECT {fn RIGHT('Hello World', 5)}"), "World");
    ASSERT_EQ(query<std::string>("SELECT {fn RIGHT('Hello', 3)}"), "llo");
    // Count exceeds length
    ASSERT_EQ(query<std::string>("SELECT {fn RIGHT('Hi', 10)}"), "Hi");
}

TEST_F(ScalarFunctionsTest, RTRIM) {
    ASSERT_EQ(query<std::string>("SELECT {fn RTRIM('Hello   ')}"), "Hello");
    ASSERT_EQ(query<std::string>("SELECT {fn RTRIM('  Hello World  ')}"), "  Hello World");
    ASSERT_EQ(query<std::string>("SELECT {fn RTRIM('NoSpaces')}"), "NoSpaces");
}

TEST_F(ScalarFunctionsTest, SOUNDEX) {
    // Classic soundex examples
    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX('Robert')}"), "R163");
    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX('Rupert')}"), "R163");
    // Same soundex for similar sounding names
    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX('Smith')}"), "S530");
    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX('Smythe')}"), "S530");
    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX('Hello')}"), "H400");
}

TEST_F(ScalarFunctionsTest, SPACE) {
    ASSERT_EQ(query<std::string>("SELECT {fn SPACE(5)}"), "     ");
    ASSERT_EQ(query<std::string>("SELECT {fn SPACE(1)}"), " ");
    ASSERT_EQ(query<std::string>("SELECT {fn SPACE(0)}"), "");
    ASSERT_EQ(query<std::string>("SELECT {fn SPACE(10)}"), "          ");
}

TEST_F(ScalarFunctionsTest, SUBSTRING) {
    // SUBSTRING(str, start, length)
    ASSERT_EQ(query<std::string>("SELECT {fn SUBSTRING('Hello World', 1, 5)}"), "Hello");
    ASSERT_EQ(query<std::string>("SELECT {fn SUBSTRING('Hello World', 7, 5)}"), "World");
    // Without length - to end of string
    ASSERT_EQ(query<std::string>("SELECT {fn SUBSTRING('Hello World', 7)}"), "World");
}

TEST_F(ScalarFunctionsTest, UCASE) {
    ASSERT_EQ(query<std::string>("SELECT {fn UCASE('Hello World')}"), "HELLO WORLD");
    ASSERT_EQ(query<std::string>("SELECT {fn UCASE('lowercase')}"), "LOWERCASE");
    ASSERT_EQ(query<std::string>("SELECT {fn UCASE('MiXeD CaSe')}"), "MIXED CASE");
}

// ============================================================================
// Numeric Functions
// ============================================================================

TEST_F(ScalarFunctionsTest, ABS) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ABS(-5)}"), 5.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ABS(5)}"), 5.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ABS(-3.14)}"), 3.14);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ABS(0)}"), 0.0);
}

TEST_F(ScalarFunctionsTest, ACOS) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ACOS(1)}"), 0.0);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn ACOS(0)}"), 1.5707963267948966, 1e-10);  // PI/2
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn ACOS(-1)}"), 3.141592653589793, 1e-10);  // PI
}

TEST_F(ScalarFunctionsTest, ASIN) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ASIN(0)}"), 0.0);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn ASIN(1)}"), 1.5707963267948966, 1e-10);  // PI/2
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn ASIN(-1)}"), -1.5707963267948966, 1e-10);  // -PI/2
}

TEST_F(ScalarFunctionsTest, ATAN) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ATAN(0)}"), 0.0);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn ATAN(1)}"), 0.7853981633974483, 1e-10);  // PI/4
}

TEST_F(ScalarFunctionsTest, ATAN2) {
    // ATAN2(y, x) - angle from x-axis to point (x, y)
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn ATAN2(1, 1)}"), 0.7853981633974483, 1e-10);  // PI/4
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ATAN2(0, 1)}"), 0.0);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn ATAN2(1, 0)}"), 1.5707963267948966, 1e-10);  // PI/2
}

TEST_F(ScalarFunctionsTest, CEILING) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn CEILING(4.2)}"), 5.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn CEILING(-4.2)}"), -4.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn CEILING(5.0)}"), 5.0);
}

TEST_F(ScalarFunctionsTest, COS) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn COS(0)}"), 1.0);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn COS({fn PI()})}"), -1.0, 1e-10);
}

/*
TEST_F(ScalarFunctionsTest, COT) {
    // COT(x) = 1/TAN(x) = COS(x)/SIN(x)
    ASSERT_NEAR(*query2<SQLDOUBLE>("SELECT {fn COT({fn PI()} / 4)}"), 1.0, 1e-10);
}
*/

TEST_F(ScalarFunctionsTest, DEGREES) {
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn DEGREES({fn PI()})}"), 180.0, 1e-10);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn DEGREES({fn PI()} / 2)}"), 90.0, 1e-10);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn DEGREES(0)}"), 0.0);
}

TEST_F(ScalarFunctionsTest, EXP) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn EXP(0)}"), 1.0);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn EXP(1)}"), 2.718281828459045, 1e-10);  // e
}

TEST_F(ScalarFunctionsTest, FLOOR) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn FLOOR(4.7)}"), 4.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn FLOOR(-4.7)}"), -5.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn FLOOR(5.0)}"), 5.0);
}

TEST_F(ScalarFunctionsTest, LOG) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn LOG(1)}"), 0.0);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn LOG({fn EXP(1)})}"), 1.0, 1e-8);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn LOG(10)}"), 2.302585092994046, 1e-8);
}

TEST_F(ScalarFunctionsTest, LOG10) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn LOG10(1)}"), 0.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn LOG10(10)}"), 1.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn LOG10(100)}"), 2.0);
}

TEST_F(ScalarFunctionsTest, MOD) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MOD(10, 3)}"), 1);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MOD(15, 5)}"), 0);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MOD(7, 2)}"), 1);
}

TEST_F(ScalarFunctionsTest, PI) {
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn PI()}"), 3.141592653589793, 1e-10);
}

TEST_F(ScalarFunctionsTest, POWER) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn POWER(2, 3)}"), 8.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn POWER(10, 2)}"), 100.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn POWER(4, 0.5)}"), 2.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn POWER(2, -1)}"), 0.5);
}

TEST_F(ScalarFunctionsTest, RADIANS) {
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn RADIANS(180)}"), 3.141592653589793, 1e-10);  // PI
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn RADIANS(90)}"), 1.5707963267948966, 1e-10);  // PI/2
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn RADIANS(0)}"), 0.0);
}

TEST_F(ScalarFunctionsTest, RAND) {
    // RAND returns value between 0 and 1
    auto result = query<SQLDOUBLE>("SELECT {fn RAND()}");
    ASSERT_TRUE(result.has_value());
    ASSERT_GE(*result, 0.0);
    ASSERT_LE(*result, 1.0);

    // These cases are not implemented:
    // 1. Deterministic RAND with a seed
    // auto result1 = query2<SQLDOUBLE>("SELECT {fn RAND(42)}");
    // auto result2 = query2<SQLDOUBLE>("SELECT {fn RAND(42)}");
    // ASSERT_EQ(result1, result2);

    // 2. Rand must produce different values even when called in the same query
    // Currently it is not the case two calls to RAND() in the same query will produce the same
    // random value.
    // ASSERT_EQ(query2<SQLINTEGER>("SELECT {fn RAND()} != {fn RAND()}"), 1);
}

TEST_F(ScalarFunctionsTest, ROUND) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ROUND(3.14159, 2)}"), 3.14);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ROUND(3.5)}"), 4.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ROUND(-3.5)}"), -4.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ROUND(1234.5678, -2)}"), 1200.0);
}

TEST_F(ScalarFunctionsTest, SIGN) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SIGN(42)}"), 1);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SIGN(-42)}"), -1);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SIGN(0)}"), 0);
}

TEST_F(ScalarFunctionsTest, SIN) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn SIN(0)}"), 0.0);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn SIN({fn PI()} / 2)}"), 1.0, 1e-10);
}

TEST_F(ScalarFunctionsTest, SQRT) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn SQRT(4)}"), 2.0);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn SQRT(2)}"), 1.4142135623730951, 1e-10);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn SQRT(0)}"), 0.0);
}

TEST_F(ScalarFunctionsTest, TAN) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TAN(0)}"), 0.0);
    ASSERT_NEAR(*query<SQLDOUBLE>("SELECT {fn TAN({fn PI()} / 4)}"), 1.0, 1e-10);
}

TEST_F(ScalarFunctionsTest, TRUNCATE) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TRUNCATE(3.14159, 2)}"), 3.14);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TRUNCATE(3.9)}"), 3.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TRUNCATE(-3.9)}"), -3.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TRUNCATE(1234.5678, -2)}"), 1200.0);
}
