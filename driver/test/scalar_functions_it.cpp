#include <gtest/gtest.h>
#include "driver/test/client_test_base.h"

class ScalarFunctionsTest
    : public ClientTestBase
{
protected:
    void query(std::string query, SQLSMALLINT type, void * out_buffer, size_t out_buffer_size, SQLLEN * indicator)
    {
        auto query_encoded = fromUTF8<PTChar>(query);
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecDirect(hstmt, ptcharCast(query_encoded.data()), SQL_NTS));

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLGetData(
            hstmt,
            1,
            type,
            out_buffer,
            out_buffer_size,
            indicator
        ));

        SQLFreeStmt(hstmt, SQL_CLOSE);
    }
};

TEST_F(ScalarFunctionsTest, ASCII) {
    SQLINTEGER code = 0;

    query("SELECT {fn ASCII('A')}", SQL_C_SLONG, &code, sizeof(code), NULL);
    ASSERT_EQ(code, 'A');

    query("SELECT {fn ASCII('abc')}", SQL_C_SLONG, &code, sizeof(code), NULL);
    ASSERT_EQ(code, 'a');  // Returns ASCII of first character 'a'

    query("SELECT {fn ASCII('0')}", SQL_C_SLONG, &code, sizeof(code), NULL);
    ASSERT_EQ(code, '0');

    query("SELECT {fn ASCII(' ')}", SQL_C_SLONG, &code, sizeof(code), NULL);
    ASSERT_EQ(code, ' ');
}

TEST_F(ScalarFunctionsTest, BIT_LENGTH) {
    SQLINTEGER len = 0;

    // ASCII: 12 chars * 1 byte * 8 bits = 96 bits
    query("SELECT {fn BIT_LENGTH('Hello World!')}", SQL_C_SLONG, &len, sizeof(len), NULL);
    ASSERT_EQ(len, 96);
}

TEST_F(ScalarFunctionsTest, CHAR) {
    SQLCHAR ch[2] = {0};

    query("SELECT {fn CHAR(65)}", SQL_C_CHAR, ch, sizeof(ch), NULL);
    ASSERT_EQ(ch[0], 'A');

    query("SELECT {fn CHAR(97)}", SQL_C_CHAR, ch, sizeof(ch), NULL);
    ASSERT_EQ(ch[0], 'a');

    query("SELECT {fn CHAR(48)}", SQL_C_CHAR, ch, sizeof(ch), NULL);
    ASSERT_EQ(ch[0], '0');

    query("SELECT {fn CHAR(32)}", SQL_C_CHAR, ch, sizeof(ch), NULL);
    ASSERT_EQ(ch[0], ' ');
}

TEST_F(ScalarFunctionsTest, CHAR_LENGTH) {
    SQLINTEGER len = 0;
    query("SELECT {fn CHAR_LENGTH('Hello World!')}", SQL_C_SLONG, &len, sizeof(len), NULL);
    ASSERT_EQ(len, 12);
}

TEST_F(ScalarFunctionsTest, CHARACTER_LENGTH) {
    SQLINTEGER len = 0;

    query("SELECT {fn CHARACTER_LENGTH('Hello World!')}", SQL_C_SLONG, &len, sizeof(len), NULL);
    ASSERT_EQ(len, 12);
}

TEST_F(ScalarFunctionsTest, CONCAT) {
    char result[64] = {0};

    query("SELECT {fn CONCAT('Hello', ' World')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello World");

    query("SELECT {fn CONCAT('foo', 'bar')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "foobar");

    query("SELECT {fn CONCAT('', 'test')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "test");
}

TEST_F(ScalarFunctionsTest, DIFFERENCE) {
    SQLINTEGER diff = 0;

    // Identical soundex codes (S530) - maximum similarity
    query("SELECT {fn DIFFERENCE('Smith', 'Smythe')}", SQL_C_SLONG, &diff, sizeof(diff), NULL);
    ASSERT_EQ(diff, 4);

    // Same word - identical
    query("SELECT {fn DIFFERENCE('Hello', 'Hello')}", SQL_C_SLONG, &diff, sizeof(diff), NULL);
    ASSERT_EQ(diff, 4);

    // Similar sounding names
    query("SELECT {fn DIFFERENCE('Green', 'Greene')}", SQL_C_SLONG, &diff, sizeof(diff), NULL);
    ASSERT_EQ(diff, 4);

    // Different words - low similarity
    query("SELECT {fn DIFFERENCE('Hello', 'Goodbye')}", SQL_C_SLONG, &diff, sizeof(diff), NULL);
    ASSERT_EQ(diff, 1);
}

TEST_F(ScalarFunctionsTest, INSERT) {
    char result[64] = {0};
    SQLLEN indicator = 0;

    // INSERT(str1, start, length, str2) - insert str2 into str1 at start, replacing length chars
    query("SELECT {fn INSERT('Hello World', 7, 5, 'There')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello There");

    query("SELECT {fn INSERT('abcdef', 3, 2, 'XYZ')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "abXYZef");

    // Insert at beginning (start=1, length=0 means pure insert)
    query("SELECT {fn INSERT('World', 1, 0, 'Hello ')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello World");

    // Edge case: start=0 (invalid position, should return original string)
    query("SELECT {fn INSERT('Hello', 0, 2, 'XYZ')}", SQL_C_CHAR, result, sizeof(result), &indicator);
    ASSERT_EQ(indicator, SQL_NULL_DATA);

    // Edge case: start beyond string length (should return original string)
    query("SELECT {fn INSERT('Hello', 10, 2, 'XYZ')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "HelloXYZ");

    // Edge case: length extends beyond string end (delete to end, then insert)
    query("SELECT {fn INSERT('Hello', 3, 100, 'XYZ')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "HeXYZ");

    // Edge case: empty str2 (just delete)
    query("SELECT {fn INSERT('Hello', 2, 3, '')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Ho");

    // Edge case: empty str1
    query("SELECT {fn INSERT('', 1, 0, 'Hello')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello");
}

TEST_F(ScalarFunctionsTest, LCASE) {
    char result[64] = {0};

    query("SELECT {fn LCASE('Hello World')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "hello world");

    query("SELECT {fn LCASE('UPPERCASE')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "uppercase");

    query("SELECT {fn LCASE('MiXeD CaSe')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "mixed case");
}

TEST_F(ScalarFunctionsTest, LEFT) {
    char result[64] = {0};

    query("SELECT {fn LEFT('Hello World', 5)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello");

    query("SELECT {fn LEFT('Hello', 3)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hel");

    query("SELECT {fn LEFT('Hi', 10)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hi");
}

TEST_F(ScalarFunctionsTest, LENGTH) {
    SQLINTEGER len = 0;
    SQLLEN indicator = 0;

    query("SELECT {fn LENGTH('Hello World!')}", SQL_C_SLONG, &len, sizeof(len), NULL);
    ASSERT_EQ(len, 12);

    query("SELECT {fn LENGTH('')}", SQL_C_SLONG, &len, sizeof(len), NULL);
    ASSERT_EQ(len, 0);

    query("SELECT {fn LENGTH(NULL)}", SQL_C_SLONG, &len, sizeof(len), &indicator);
    ASSERT_EQ(indicator, SQL_NULL_DATA);
}

TEST_F(ScalarFunctionsTest, LOCATE) {
    SQLINTEGER pos = 0;

    query("SELECT {fn LOCATE('World', 'Hello World')}", SQL_C_SLONG, &pos, sizeof(pos), NULL);
    ASSERT_EQ(pos, 7);

    query("SELECT {fn LOCATE('o', 'Hello World')}", SQL_C_SLONG, &pos, sizeof(pos), NULL);
    ASSERT_EQ(pos, 5);

    // With start position - find second 'o'
    query("SELECT {fn LOCATE('o', 'Hello World', 6)}", SQL_C_SLONG, &pos, sizeof(pos), NULL);
    ASSERT_EQ(pos, 8);

    // Not found
    query("SELECT {fn LOCATE('xyz', 'Hello World')}", SQL_C_SLONG, &pos, sizeof(pos), NULL);
    ASSERT_EQ(pos, 0);
}

TEST_F(ScalarFunctionsTest, LTRIM) {
    char result[64] = {0};

    query("SELECT {fn LTRIM('   Hello')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello");

    query("SELECT {fn LTRIM('  Hello World  ')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello World  ");

    query("SELECT {fn LTRIM('NoSpaces')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "NoSpaces");
}

TEST_F(ScalarFunctionsTest, OCTET_LENGTH) {
    SQLINTEGER len = 0;

    query("SELECT {fn OCTET_LENGTH('Hello')}", SQL_C_SLONG, &len, sizeof(len), NULL);
    ASSERT_EQ(len, 5);

    query("SELECT {fn OCTET_LENGTH('')}", SQL_C_SLONG, &len, sizeof(len), NULL);
    ASSERT_EQ(len, 0);
}

TEST_F(ScalarFunctionsTest, POSITION) {
    SQLINTEGER pos = 0;

    // POSITION(substr IN str) - same as LOCATE but different syntax
    query("SELECT {fn POSITION('World' IN 'Hello World')}", SQL_C_SLONG, &pos, sizeof(pos), NULL);
    ASSERT_EQ(pos, 7);

    query("SELECT {fn POSITION('o' IN 'Hello')}", SQL_C_SLONG, &pos, sizeof(pos), NULL);
    ASSERT_EQ(pos, 5);

    // Not found
    query("SELECT {fn POSITION('xyz' IN 'Hello')}", SQL_C_SLONG, &pos, sizeof(pos), NULL);
    ASSERT_EQ(pos, 0);
}

TEST_F(ScalarFunctionsTest, REPEAT) {
    char result[64] = {0};

    query("SELECT {fn REPEAT('ab', 3)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "ababab");

    query("SELECT {fn REPEAT('Hello', 2)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "HelloHello");

    query("SELECT {fn REPEAT('x', 5)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "xxxxx");

    // Zero repeats
    query("SELECT {fn REPEAT('Hello', 0)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "");
}

TEST_F(ScalarFunctionsTest, REPLACE) {
    char result[64] = {0};

    query("SELECT {fn REPLACE('Hello World', 'World', 'There')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello There");

    // Replace all occurrences
    query("SELECT {fn REPLACE('ababa', 'a', 'x')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "xbxbx");

    // No match
    query("SELECT {fn REPLACE('Hello', 'xyz', 'abc')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello");

    // Replace with empty string (delete)
    query("SELECT {fn REPLACE('Hello World', ' World', '')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello");
}

TEST_F(ScalarFunctionsTest, RIGHT) {
    char result[64] = {0};

    query("SELECT {fn RIGHT('Hello World', 5)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "World");

    query("SELECT {fn RIGHT('Hello', 3)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "llo");

    // Count exceeds length
    query("SELECT {fn RIGHT('Hi', 10)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hi");
}

TEST_F(ScalarFunctionsTest, RTRIM) {
    char result[64] = {0};

    query("SELECT {fn RTRIM('Hello   ')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello");

    query("SELECT {fn RTRIM('  Hello World  ')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "  Hello World");

    query("SELECT {fn RTRIM('NoSpaces')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "NoSpaces");
}

TEST_F(ScalarFunctionsTest, SOUNDEX) {
    char result[16] = {0};

    // Classic soundex examples
    query("SELECT {fn SOUNDEX('Robert')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "R163");

    query("SELECT {fn SOUNDEX('Rupert')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "R163");

    // Same soundex for similar sounding names
    query("SELECT {fn SOUNDEX('Smith')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "S530");

    query("SELECT {fn SOUNDEX('Smythe')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "S530");

    query("SELECT {fn SOUNDEX('Hello')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "H400");
}

TEST_F(ScalarFunctionsTest, SPACE) {
    char result[64] = {0};

    query("SELECT {fn SPACE(5)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "     ");

    query("SELECT {fn SPACE(1)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, " ");

    query("SELECT {fn SPACE(0)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "");

    // Verify length
    query("SELECT {fn SPACE(10)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "          ");
}

TEST_F(ScalarFunctionsTest, SUBSTRING) {
    char result[64] = {0};

    // SUBSTRING(str, start, length)
    query("SELECT {fn SUBSTRING('Hello World', 1, 5)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "Hello");

    query("SELECT {fn SUBSTRING('Hello World', 7, 5)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "World");

    // Without length - to end of string
    query("SELECT {fn SUBSTRING('Hello World', 7)}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "World");
}

TEST_F(ScalarFunctionsTest, UCASE) {
    char result[64] = {0};

    query("SELECT {fn UCASE('Hello World')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "HELLO WORLD");

    query("SELECT {fn UCASE('lowercase')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "LOWERCASE");

    query("SELECT {fn UCASE('MiXeD CaSe')}", SQL_C_CHAR, result, sizeof(result), NULL);
    ASSERT_STREQ(result, "MIXED CASE");
}

// ============================================================================
// Numeric Functions
// ============================================================================

TEST_F(ScalarFunctionsTest, ABS) {
    SQLDOUBLE result = 0;

    query("SELECT {fn ABS(-5)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 5.0);

    query("SELECT {fn ABS(5)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 5.0);

    query("SELECT {fn ABS(-3.14)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 3.14);

    query("SELECT {fn ABS(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);
}

TEST_F(ScalarFunctionsTest, ACOS) {
    SQLDOUBLE result = 0;

    query("SELECT {fn ACOS(1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);

    query("SELECT {fn ACOS(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 1.5707963267948966, 1e-10);  // PI/2

    query("SELECT {fn ACOS(-1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 3.141592653589793, 1e-10);  // PI
}

TEST_F(ScalarFunctionsTest, ASIN) {
    SQLDOUBLE result = 0;

    query("SELECT {fn ASIN(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);

    query("SELECT {fn ASIN(1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 1.5707963267948966, 1e-10);  // PI/2

    query("SELECT {fn ASIN(-1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, -1.5707963267948966, 1e-10);  // -PI/2
}

TEST_F(ScalarFunctionsTest, ATAN) {
    SQLDOUBLE result = 0;

    query("SELECT {fn ATAN(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);

    query("SELECT {fn ATAN(1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 0.7853981633974483, 1e-10);  // PI/4
}

TEST_F(ScalarFunctionsTest, ATAN2) {
    SQLDOUBLE result = 0;

    // ATAN2(y, x) - angle from x-axis to point (x, y)
    query("SELECT {fn ATAN2(1, 1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 0.7853981633974483, 1e-10);  // PI/4

    query("SELECT {fn ATAN2(0, 1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);

    query("SELECT {fn ATAN2(1, 0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 1.5707963267948966, 1e-10);  // PI/2
}

TEST_F(ScalarFunctionsTest, CEILING) {
    SQLDOUBLE result = 0;

    query("SELECT {fn CEILING(4.2)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 5.0);

    query("SELECT {fn CEILING(-4.2)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, -4.0);

    query("SELECT {fn CEILING(5.0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 5.0);
}

TEST_F(ScalarFunctionsTest, COS) {
    SQLDOUBLE result = 0;

    query("SELECT {fn COS(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 1.0);

    query("SELECT {fn COS({fn PI()})}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, -1.0, 1e-10);
}

/*
TEST_F(ScalarFunctionsTest, COT) {
    SQLDOUBLE result = 0;

    // COT(x) = 1/TAN(x) = COS(x)/SIN(x)
    query("SELECT {fn COT({fn PI()} / 4)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 1.0, 1e-10);
}
*/

TEST_F(ScalarFunctionsTest, DEGREES) {
    SQLDOUBLE result = 0;

    query("SELECT {fn DEGREES({fn PI()})}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 180.0, 1e-10);

    query("SELECT {fn DEGREES({fn PI()} / 2)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 90.0, 1e-10);

    query("SELECT {fn DEGREES(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);
}

TEST_F(ScalarFunctionsTest, EXP) {
    SQLDOUBLE result = 0;

    query("SELECT {fn EXP(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 1.0);

    query("SELECT {fn EXP(1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 2.718281828459045, 1e-10);  // e
}

TEST_F(ScalarFunctionsTest, FLOOR) {
    SQLDOUBLE result = 0;

    query("SELECT {fn FLOOR(4.7)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 4.0);

    query("SELECT {fn FLOOR(-4.7)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, -5.0);

    query("SELECT {fn FLOOR(5.0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 5.0);
}

TEST_F(ScalarFunctionsTest, LOG) {
    SQLDOUBLE result = 0;

    query("SELECT {fn LOG(1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);

    query("SELECT {fn LOG({fn EXP(1)})}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 1.0, 1e-8);

    query("SELECT {fn LOG(10)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 2.302585092994046, 1e-8);
}

TEST_F(ScalarFunctionsTest, LOG10) {
    SQLDOUBLE result = 0;

    query("SELECT {fn LOG10(1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);

    query("SELECT {fn LOG10(10)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 1.0);

    query("SELECT {fn LOG10(100)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 2.0);
}

TEST_F(ScalarFunctionsTest, MOD) {
    SQLINTEGER result = 0;

    query("SELECT {fn MOD(10, 3)}", SQL_C_SLONG, &result, sizeof(result), NULL);
    ASSERT_EQ(result, 1);

    query("SELECT {fn MOD(15, 5)}", SQL_C_SLONG, &result, sizeof(result), NULL);
    ASSERT_EQ(result, 0);

    query("SELECT {fn MOD(7, 2)}", SQL_C_SLONG, &result, sizeof(result), NULL);
    ASSERT_EQ(result, 1);
}

TEST_F(ScalarFunctionsTest, PI) {
    SQLDOUBLE result = 0;

    query("SELECT {fn PI()}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 3.141592653589793, 1e-10);
}

TEST_F(ScalarFunctionsTest, POWER) {
    SQLDOUBLE result = 0;

    query("SELECT {fn POWER(2, 3)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 8.0);

    query("SELECT {fn POWER(10, 2)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 100.0);

    query("SELECT {fn POWER(4, 0.5)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 2.0);

    query("SELECT {fn POWER(2, -1)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.5);
}

TEST_F(ScalarFunctionsTest, RADIANS) {
    SQLDOUBLE result = 0;

    query("SELECT {fn RADIANS(180)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 3.141592653589793, 1e-10);  // PI

    query("SELECT {fn RADIANS(90)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 1.5707963267948966, 1e-10);  // PI/2

    query("SELECT {fn RADIANS(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);
}

TEST_F(ScalarFunctionsTest, RAND) {
    SQLDOUBLE result = 0;

    // RAND returns value between 0 and 1
    query("SELECT {fn RAND()}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_GE(result, 0.0);
    ASSERT_LE(result, 1.0);

    // These cases are not implemented:
    // 1. Deterministic RAND with a seed
    // SQLDOUBLE result1 = 0, result2 = 0;
    // query("SELECT {fn RAND(42)}", SQL_C_DOUBLE, &result1, sizeof(result1), NULL);
    // query("SELECT {fn RAND(42)}", SQL_C_DOUBLE, &result2, sizeof(result2), NULL);
    // ASSERT_DOUBLE_EQ(result1, result2);

    // 2. Rand must produce different values even when called in the same query
    // Currently it is not the case two calls to RAND() in the same query will produce the same
    // random value.
    // SQLINTEGER different = 0;
    // query("SELECT {fn RAND()} != {fn RAND()}", SQL_C_SLONG, &different, sizeof(different), NULL);
    // ASSERT_EQ(different, 1);
}

TEST_F(ScalarFunctionsTest, ROUND) {
    SQLDOUBLE result = 0;

    query("SELECT {fn ROUND(3.14159, 2)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 3.14);

    query("SELECT {fn ROUND(3.5)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 4.0);

    query("SELECT {fn ROUND(-3.5)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, -4.0);

    query("SELECT {fn ROUND(1234.5678, -2)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 1200.0);
}

TEST_F(ScalarFunctionsTest, SIGN) {
    SQLINTEGER result = 0;

    query("SELECT {fn SIGN(42)}", SQL_C_SLONG, &result, sizeof(result), NULL);
    ASSERT_EQ(result, 1);

    query("SELECT {fn SIGN(-42)}", SQL_C_SLONG, &result, sizeof(result), NULL);
    ASSERT_EQ(result, -1);

    query("SELECT {fn SIGN(0)}", SQL_C_SLONG, &result, sizeof(result), NULL);
    ASSERT_EQ(result, 0);
}

TEST_F(ScalarFunctionsTest, SIN) {
    SQLDOUBLE result = 0;

    query("SELECT {fn SIN(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);

    query("SELECT {fn SIN({fn PI()} / 2)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 1.0, 1e-10);
}

TEST_F(ScalarFunctionsTest, SQRT) {
    SQLDOUBLE result = 0;

    query("SELECT {fn SQRT(4)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 2.0);

    query("SELECT {fn SQRT(2)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 1.4142135623730951, 1e-10);

    query("SELECT {fn SQRT(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);
}

TEST_F(ScalarFunctionsTest, TAN) {
    SQLDOUBLE result = 0;

    query("SELECT {fn TAN(0)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 0.0);

    query("SELECT {fn TAN({fn PI()} / 4)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_NEAR(result, 1.0, 1e-10);
}

TEST_F(ScalarFunctionsTest, TRUNCATE) {
    SQLDOUBLE result = 0;

    query("SELECT {fn TRUNCATE(3.14159, 2)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 3.14);

    query("SELECT {fn TRUNCATE(3.9)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 3.0);

    query("SELECT {fn TRUNCATE(-3.9)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, -3.0);

    query("SELECT {fn TRUNCATE(1234.5678, -2)}", SQL_C_DOUBLE, &result, sizeof(result), NULL);
    ASSERT_DOUBLE_EQ(result, 1200.0);
}

