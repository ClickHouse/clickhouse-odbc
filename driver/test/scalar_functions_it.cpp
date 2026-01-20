#include <gtest/gtest.h>
#include "driver/test/client_test_base.h"
#include "driver/test/date_utils.h"

class ScalarFunctionsTest
    : public ClientTestBase
{
protected:
    // Unified query function:
    //   query<SQLINTEGER>("SELECT ...") - returns single value
    //   query<std::tuple<SQLINTEGER, std::string>>("SELECT ...") - returns tuple
    template <typename SqlType, typename... Params>
    SqlType query(const std::string & query_str, Params... params)
    {
        prepare(query_str);

        // Convert and store params in a tuple to ensure their lifetime spans SQLExecute
        auto storage = std::make_tuple(static_cast<typename SqlBindType<Params>::type>(params)...);
        bindFromTuple(storage, std::index_sequence_for<Params...>{});

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLExecute(hstmt));
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFetch(hstmt));

        SqlType res;
        if constexpr (is_tuple<SqlType>::value) {
            res = getDataTuple<SqlType>(std::make_index_sequence<std::tuple_size_v<SqlType>>{});
        } else {
            res = getData<SqlType>(1);
        }

        ODBC_CALL_ON_STMT_THROW(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));
        return res;
    }

private:
    // Type trait to detect std::tuple
    template <typename T>
    struct is_tuple : std::false_type {};

    template <typename... Ts>
    struct is_tuple<std::tuple<Ts...>> : std::true_type {};

    // Type trait to detect std::optional
    template <typename T>
    struct is_optional : std::false_type {};

    template <typename T>
    struct is_optional<std::optional<T>> : std::true_type {};

    // Helper to fetch multiple columns as a tuple
    template <typename Tuple, size_t... Is>
    Tuple getDataTuple(std::index_sequence<Is...>)
    {
        return Tuple{getData<std::tuple_element_t<Is, Tuple>>(Is + 1)...};
    }

    void prepare(const std::string & query)
    {
        auto query_encoded = fromUTF8<PTChar>(query);
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLPrepare(hstmt, ptcharCast(query_encoded.data()), SQL_NTS));
    }

    // Core getData implementation - returns std::optional, never throws on NULL
    template <typename SqlType>
    std::optional<SqlType> getDataOptional(SQLUSMALLINT idx)
    {
        SqlType buffer{};
        SQLLEN indicator;
        ODBC_CALL_ON_STMT_THROW(
            hstmt, SQLGetData(hstmt, idx, getCTypeFor<SqlType>(), &buffer, sizeof(SqlType), &indicator));
        if (indicator == SQL_NULL_DATA) {
            return std::nullopt;
        }
        return buffer;
    }

    template <>
    std::optional<std::string> getDataOptional(SQLUSMALLINT idx)
    {
        static const size_t max_string_size = 1024;
        std::string buffer(max_string_size, '\0');
        SQLLEN indicator;
        ODBC_CALL_ON_STMT_THROW(
            hstmt, SQLGetData(hstmt, idx, SQL_C_CHAR, buffer.data(), buffer.size(), &indicator));
        if (indicator == SQL_NULL_DATA) {
            return std::nullopt;
        }
        assert(indicator >= 0 && "cannot read size from a negative indicator");
        buffer.resize(indicator);
        return buffer;
    }

    template <>
    std::optional<SQL_DATE_STRUCT> getDataOptional(SQLUSMALLINT idx)
    {
        SQL_DATE_STRUCT buffer{};
        SQLLEN indicator;
        ODBC_CALL_ON_STMT_THROW(
            hstmt, SQLGetData(hstmt, idx, SQL_C_TYPE_DATE, &buffer, sizeof(buffer), &indicator));
        if (indicator == SQL_NULL_DATA) {
            return std::nullopt;
        }
        return buffer;
    }

    template <>
    std::optional<SQL_TIME_STRUCT> getDataOptional(SQLUSMALLINT idx)
    {
        SQL_TIME_STRUCT buffer{};
        SQLLEN indicator;
        ODBC_CALL_ON_STMT_THROW(
            hstmt, SQLGetData(hstmt, idx, SQL_C_TYPE_TIME, &buffer, sizeof(buffer), &indicator));
        if (indicator == SQL_NULL_DATA) {
            return std::nullopt;
        }
        return buffer;
    }

    template <>
    std::optional<SQL_TIMESTAMP_STRUCT> getDataOptional(SQLUSMALLINT idx)
    {
        SQL_TIMESTAMP_STRUCT buffer{};
        SQLLEN indicator;
        ODBC_CALL_ON_STMT_THROW(
            hstmt, SQLGetData(hstmt, idx, SQL_C_TYPE_TIMESTAMP, &buffer, sizeof(buffer), &indicator));
        if (indicator == SQL_NULL_DATA) {
            return std::nullopt;
        }
        return buffer;
    }

    // Main getData - handles both optional and non-optional types
    //   getData<SQLINTEGER>(1) - throws if NULL
    //   getData<std::optional<SQLINTEGER>>(1) - returns std::nullopt if NULL
    template <typename SqlType>
    SqlType getData(SQLUSMALLINT idx)
    {
        if constexpr (is_optional<SqlType>::value) {
            // SqlType is std::optional<T> - return nullopt on NULL
            using InnerType = typename SqlType::value_type;
            return getDataOptional<InnerType>(idx);
        } else {
            // SqlType is not optional - throw on NULL
            auto result = getDataOptional<SqlType>(idx);
            if (!result) {
                throw std::runtime_error("NULL value");
            }
            return *result;
        }
    }

    void bind(SQLUSMALLINT idx, const char ** value)
    {
        static SQLLEN len = SQL_NTS;
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
            /* StatementHandle   */ hstmt,
            /* ParameterNumber   */ idx,
            /* InputOutputType   */ SQL_PARAM_INPUT,
            /* ValueType         */ SQL_C_CHAR,
            /* ParameterType     */ SQL_VARCHAR,
            /* ColumnSize        */ static_cast<SQLULEN>(strlen(*value)),
            /* DecimalDigits     */ 0,
            /* ParameterValuePtr */ const_cast<char *>(*value),
            /* BufferLength      */ static_cast<SQLLEN>(strlen(*value)),
            /* StrLen_or_IndPtr  */ &len
        ));
    }

    void bind(SQLUSMALLINT idx, SQLINTEGER * value)
    {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
            /* StatementHandle   */ hstmt,
            /* ParameterNumber   */ idx,
            /* InputOutputType   */ SQL_PARAM_INPUT,
            /* ValueType         */ SQL_C_SLONG,
            /* ParameterType     */ SQL_INTEGER,
            /* ColumnSize        */ 0,
            /* DecimalDigits     */ 0,
            /* ParameterValuePtr */ value,
            /* BufferLength      */ 0,
            /* StrLen_or_IndPtr  */ nullptr
        ));
    }

    void bind(SQLUSMALLINT idx, SQLDOUBLE * value)
    {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
            /* StatementHandle   */ hstmt,
            /* ParameterNumber   */ idx,
            /* InputOutputType   */ SQL_PARAM_INPUT,
            /* ValueType         */ SQL_C_DOUBLE,
            /* ParameterType     */ SQL_DOUBLE,
            /* ColumnSize        */ 0,
            /* DecimalDigits     */ 0,
            /* ParameterValuePtr */ value,
            /* BufferLength      */ 0,
            /* StrLen_or_IndPtr  */ nullptr
        ));
    }

    void bind(SQLUSMALLINT idx, SQL_DATE_STRUCT * value)
    {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
            /* StatementHandle   */ hstmt,
            /* ParameterNumber   */ idx,
            /* InputOutputType   */ SQL_PARAM_INPUT,
            /* ValueType         */ SQL_C_TYPE_DATE,
            /* ParameterType     */ SQL_TYPE_DATE,
            /* ColumnSize        */ 0,
            /* DecimalDigits     */ 0,
            /* ParameterValuePtr */ value,
            /* BufferLength      */ sizeof(SQL_DATE_STRUCT),
            /* StrLen_or_IndPtr  */ nullptr
        ));
    }

    void bind(SQLUSMALLINT idx, SQL_TIME_STRUCT * value)
    {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
            /* StatementHandle   */ hstmt,
            /* ParameterNumber   */ idx,
            /* InputOutputType   */ SQL_PARAM_INPUT,
            /* ValueType         */ SQL_C_TYPE_TIME,
            /* ParameterType     */ SQL_TYPE_TIME,
            /* ColumnSize        */ 0,
            /* DecimalDigits     */ 0,
            /* ParameterValuePtr */ value,
            /* BufferLength      */ sizeof(SQL_TIME_STRUCT),
            /* StrLen_or_IndPtr  */ nullptr
        ));
    }

    void bind(SQLUSMALLINT idx, SQL_TIMESTAMP_STRUCT * value)
    {
        ODBC_CALL_ON_STMT_THROW(hstmt, SQLBindParameter(
            /* StatementHandle   */ hstmt,
            /* ParameterNumber   */ idx,
            /* InputOutputType   */ SQL_PARAM_INPUT,
            /* ValueType         */ SQL_C_TYPE_TIMESTAMP,
            /* ParameterType     */ SQL_TYPE_TIMESTAMP,
            /* ColumnSize        */ 0,
            /* DecimalDigits     */ 0,
            /* ParameterValuePtr */ value,
            /* BufferLength      */ sizeof(SQL_TIMESTAMP_STRUCT),
            /* StrLen_or_IndPtr  */ nullptr
        ));
    }

    // Map C++ types to SQL bind types (no default - unsupported types won't compile)
    template <typename T>
    struct SqlBindType;

    template <>
    struct SqlBindType<const char *> { using type = const char *; };

    template <>
    struct SqlBindType<int> { using type = SQLINTEGER; };

    template <>
    struct SqlBindType<long> { using type = SQLINTEGER; };

    template <>
    struct SqlBindType<float> { using type = SQLDOUBLE; };

    template <>
    struct SqlBindType<double> { using type = SQLDOUBLE; };

    template <>
    struct SqlBindType<SQL_DATE_STRUCT> { using type = SQL_DATE_STRUCT; };

    template <>
    struct SqlBindType<SQL_TIME_STRUCT> { using type = SQL_TIME_STRUCT; };

    template <>
    struct SqlBindType<SQL_TIMESTAMP_STRUCT> { using type = SQL_TIMESTAMP_STRUCT; };

    template <typename Tuple, size_t... Is>
    void bindFromTuple(Tuple & t, std::index_sequence<Is...>)
    {
        (bind(static_cast<SQLUSMALLINT>(Is + 1), &std::get<Is>(t)), ...);
    }
};

TEST_F(ScalarFunctionsTest, ASCII) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn ASCII('A')}"), 'A');
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn ASCII('abc')}"), 'a');  // Returns ASCII of first character 'a'
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn ASCII('0')}"), '0');
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn ASCII(' ')}"), ' ');

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn ASCII(?)}", "A"), 'A');
}

TEST_F(ScalarFunctionsTest, BIT_LENGTH) {
    // ASCII: 12 chars * 1 byte * 8 bits = 96 bits
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn BIT_LENGTH('Hello World!')}"), 96);

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn BIT_LENGTH(?)}", "Hello World!"), 96);
}

TEST_F(ScalarFunctionsTest, CHAR) {
    ASSERT_EQ(query<std::string>("SELECT {fn CHAR(65)}"), "A");
    ASSERT_EQ(query<std::string>("SELECT {fn CHAR(97)}"), "a");
    ASSERT_EQ(query<std::string>("SELECT {fn CHAR(48)}"), "0");
    ASSERT_EQ(query<std::string>("SELECT {fn CHAR(32)}"), " ");

    ASSERT_EQ(query<std::string>("SELECT {fn CHAR(?)}", 65), "A");
}

TEST_F(ScalarFunctionsTest, CHAR_LENGTH) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn CHAR_LENGTH('Hello World!')}"), 12);

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn CHAR_LENGTH(?)}", "Hello World!"), 12);
}

TEST_F(ScalarFunctionsTest, CHARACTER_LENGTH) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn CHARACTER_LENGTH('Hello World!')}"), 12);

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn CHARACTER_LENGTH(?)}", "Hello World!"), 12);
}

TEST_F(ScalarFunctionsTest, CONCAT) {
    ASSERT_EQ(query<std::string>("SELECT {fn CONCAT('Hello', ' World')}"), "Hello World");
    ASSERT_EQ(query<std::string>("SELECT {fn CONCAT('foo', 'bar')}"), "foobar");
    ASSERT_EQ(query<std::string>("SELECT {fn CONCAT('', 'test')}"), "test");

    ASSERT_EQ(query<std::string>("SELECT {fn CONCAT(?, ?)}", "Hello", " World"), "Hello World");
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

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DIFFERENCE(?, ?)}", "Smith", "Smythe"), 4);
}

TEST_F(ScalarFunctionsTest, INSERT) {
    // INSERT(str1, start, length, str2) - insert str2 into str1 at start, replacing length chars
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('Hello World', 7, 5, 'There')}"), "Hello There");
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('abcdef', 3, 2, 'XYZ')}"), "abXYZef");

    // Insert at beginning (start=1, length=0 means pure insert)
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('World', 1, 0, 'Hello ')}"), "Hello World");

    // Edge case: start beyond string length
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('Hello', 10, 2, 'XYZ')}"), "HelloXYZ");

    // Edge case: length extends beyond string end (delete to end, then insert)
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('Hello', 3, 100, 'XYZ')}"), "HeXYZ");

    // Edge case: empty str2 (just delete)
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('Hello', 2, 3, '')}"), "Ho");

    // Edge case: empty str1
    ASSERT_EQ(query<std::string>("SELECT {fn INSERT('', 1, 0, 'Hello')}"), "Hello");

    ASSERT_EQ(query<std::string>("SELECT {fn INSERT(?, ?, ?, ?)}", "Hello World", 7, 5, "There"), "Hello There");
}

TEST_F(ScalarFunctionsTest, LCASE) {
    ASSERT_EQ(query<std::string>("SELECT {fn LCASE('Hello World')}"), "hello world");
    ASSERT_EQ(query<std::string>("SELECT {fn LCASE('UPPERCASE')}"), "uppercase");
    ASSERT_EQ(query<std::string>("SELECT {fn LCASE('MiXeD CaSe')}"), "mixed case");

    ASSERT_EQ(query<std::string>("SELECT {fn LCASE(?)}", "Hello World"), "hello world");
}

TEST_F(ScalarFunctionsTest, LEFT) {
    ASSERT_EQ(query<std::string>("SELECT {fn LEFT('Hello World', 5)}"), "Hello");
    ASSERT_EQ(query<std::string>("SELECT {fn LEFT('Hello', 3)}"), "Hel");
    ASSERT_EQ(query<std::string>("SELECT {fn LEFT('Hi', 10)}"), "Hi");

    ASSERT_EQ(query<std::string>("SELECT {fn LEFT(?, ?)}", "Hello World", 5), "Hello");
}

TEST_F(ScalarFunctionsTest, LENGTH) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LENGTH('Hello World!')}"), 12);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LENGTH('')}"), 0);
    ASSERT_EQ(query<std::optional<SQLINTEGER>>("SELECT {fn LENGTH(NULL)}"), std::nullopt);

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LENGTH(?)}", "Hello World!"), 12);
}

TEST_F(ScalarFunctionsTest, LOCATE) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LOCATE('World', 'Hello World')}"), 7);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LOCATE('o', 'Hello World')}"), 5);
    // With start position - find second 'o'
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LOCATE('o', 'Hello World', 6)}"), 8);
    // Not found
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LOCATE('xyz', 'Hello World')}"), 0);

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LOCATE(?, ?)}", "World", "Hello World"), 7);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn LOCATE(?, ?, ?)}", "o", "Hello World", 6), 8);
}

TEST_F(ScalarFunctionsTest, LTRIM) {
    ASSERT_EQ(query<std::string>("SELECT {fn LTRIM('   Hello')}"), "Hello");
    ASSERT_EQ(query<std::string>("SELECT {fn LTRIM('  Hello World  ')}"), "Hello World  ");
    ASSERT_EQ(query<std::string>("SELECT {fn LTRIM('NoSpaces')}"), "NoSpaces");

    ASSERT_EQ(query<std::string>("SELECT {fn LTRIM(?)}", "   Hello"), "Hello");
}

TEST_F(ScalarFunctionsTest, OCTET_LENGTH) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn OCTET_LENGTH('Hello')}"), 5);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn OCTET_LENGTH('')}"), 0);

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn OCTET_LENGTH(?)}", "Hello"), 5);
}

TEST_F(ScalarFunctionsTest, POSITION) {
    // POSITION(substr IN str) - same as LOCATE but different syntax
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn POSITION('World' IN 'Hello World')}"), 7);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn POSITION('o' IN 'Hello')}"), 5);
    // Not found
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn POSITION('xyz' IN 'Hello')}"), 0);

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn POSITION(? IN ?)}", "World", "Hello World"), 7);
}

TEST_F(ScalarFunctionsTest, REPEAT) {
    ASSERT_EQ(query<std::string>("SELECT {fn REPEAT('ab', 3)}"), "ababab");
    ASSERT_EQ(query<std::string>("SELECT {fn REPEAT('Hello', 2)}"), "HelloHello");
    ASSERT_EQ(query<std::string>("SELECT {fn REPEAT('x', 5)}"), "xxxxx");
    // Zero repeats
    ASSERT_EQ(query<std::string>("SELECT {fn REPEAT('Hello', 0)}"), "");

    ASSERT_EQ(query<std::string>("SELECT {fn REPEAT(?, ?)}", "ab", 3), "ababab");
}

TEST_F(ScalarFunctionsTest, REPLACE) {
    ASSERT_EQ(query<std::string>("SELECT {fn REPLACE('Hello World', 'World', 'There')}"), "Hello There");
    // Replace all occurrences
    ASSERT_EQ(query<std::string>("SELECT {fn REPLACE('ababa', 'a', 'x')}"), "xbxbx");
    // No match
    ASSERT_EQ(query<std::string>("SELECT {fn REPLACE('Hello', 'xyz', 'abc')}"), "Hello");
    // Replace with empty string (delete)
    ASSERT_EQ(query<std::string>("SELECT {fn REPLACE('Hello World', ' World', '')}"), "Hello");

    ASSERT_EQ(query<std::string>("SELECT {fn REPLACE(?, ?, ?)}", "Hello World", "World", "There"), "Hello There");
}

TEST_F(ScalarFunctionsTest, RIGHT) {
    ASSERT_EQ(query<std::string>("SELECT {fn RIGHT('Hello World', 5)}"), "World");
    ASSERT_EQ(query<std::string>("SELECT {fn RIGHT('Hello', 3)}"), "llo");
    // Count exceeds length
    ASSERT_EQ(query<std::string>("SELECT {fn RIGHT('Hi', 10)}"), "Hi");

    ASSERT_EQ(query<std::string>("SELECT {fn RIGHT(?, ?)}", "Hello World", 5), "World");
}

TEST_F(ScalarFunctionsTest, RTRIM) {
    ASSERT_EQ(query<std::string>("SELECT {fn RTRIM('Hello   ')}"), "Hello");
    ASSERT_EQ(query<std::string>("SELECT {fn RTRIM('  Hello World  ')}"), "  Hello World");
    ASSERT_EQ(query<std::string>("SELECT {fn RTRIM('NoSpaces')}"), "NoSpaces");

    ASSERT_EQ(query<std::string>("SELECT {fn RTRIM(?)}", "Hello   "), "Hello");
}

TEST_F(ScalarFunctionsTest, SOUNDEX) {
    // Classic soundex examples
    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX('Robert')}"), "R163");
    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX('Rupert')}"), "R163");
    // Same soundex for similar sounding names
    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX('Smith')}"), "S530");
    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX('Smythe')}"), "S530");
    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX('Hello')}"), "H400");

    ASSERT_EQ(query<std::string>("SELECT {fn SOUNDEX(?)}", "Robert"), "R163");
}

TEST_F(ScalarFunctionsTest, SPACE) {
    ASSERT_EQ(query<std::string>("SELECT {fn SPACE(5)}"), "     ");
    ASSERT_EQ(query<std::string>("SELECT {fn SPACE(1)}"), " ");
    ASSERT_EQ(query<std::string>("SELECT {fn SPACE(0)}"), "");
    ASSERT_EQ(query<std::string>("SELECT {fn SPACE(10)}"), "          ");

    ASSERT_EQ(query<std::string>("SELECT {fn SPACE(?)}", 5), "     ");
}

TEST_F(ScalarFunctionsTest, SUBSTRING) {
    // SUBSTRING(str, start, length)
    ASSERT_EQ(query<std::string>("SELECT {fn SUBSTRING('Hello World', 1, 5)}"), "Hello");
    ASSERT_EQ(query<std::string>("SELECT {fn SUBSTRING('Hello World', 7, 5)}"), "World");
    // Without length - to end of string
    ASSERT_EQ(query<std::string>("SELECT {fn SUBSTRING('Hello World', 7)}"), "World");

    ASSERT_EQ(query<std::string>("SELECT {fn SUBSTRING(?, ?, ?)}", "Hello World", 1, 5), "Hello");
}

TEST_F(ScalarFunctionsTest, UCASE) {
    ASSERT_EQ(query<std::string>("SELECT {fn UCASE('Hello World')}"), "HELLO WORLD");
    ASSERT_EQ(query<std::string>("SELECT {fn UCASE('lowercase')}"), "LOWERCASE");
    ASSERT_EQ(query<std::string>("SELECT {fn UCASE('MiXeD CaSe')}"), "MIXED CASE");

    ASSERT_EQ(query<std::string>("SELECT {fn UCASE(?)}", "Hello World"), "HELLO WORLD");
}

// ============================================================================
// Numeric Functions
// ============================================================================

TEST_F(ScalarFunctionsTest, ABS) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ABS(-5)}"), 5.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ABS(5)}"), 5.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ABS(-3.14)}"), 3.14);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ABS(0)}"), 0.0);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ABS(?)}", -3.14), 3.14);
}

TEST_F(ScalarFunctionsTest, ACOS) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ACOS(1)}"), 0.0);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn ACOS(0)}"), 1.5707963267948966, 1e-10);  // PI/2
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn ACOS(-1)}"), 3.141592653589793, 1e-10);  // PI

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ACOS(?)}", 1.0), 0.0);
}

TEST_F(ScalarFunctionsTest, ASIN) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ASIN(0)}"), 0.0);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn ASIN(1)}"), 1.5707963267948966, 1e-10);  // PI/2
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn ASIN(-1)}"), -1.5707963267948966, 1e-10);  // -PI/2

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ASIN(?)}", 0.0), 0.0);
}

TEST_F(ScalarFunctionsTest, ATAN) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ATAN(0)}"), 0.0);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn ATAN(1)}"), 0.7853981633974483, 1e-10);  // PI/4

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ATAN(?)}", 0.0), 0.0);
}

TEST_F(ScalarFunctionsTest, ATAN2) {
    // ATAN2(y, x) - angle from x-axis to point (x, y)
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn ATAN2(1, 1)}"), 0.7853981633974483, 1e-10);  // PI/4
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ATAN2(0, 1)}"), 0.0);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn ATAN2(1, 0)}"), 1.5707963267948966, 1e-10);  // PI/2

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ATAN2(?, ?)}", 0.0, 1.0), 0.0);
}

TEST_F(ScalarFunctionsTest, CEILING) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn CEILING(4.2)}"), 5.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn CEILING(-4.2)}"), -4.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn CEILING(5.0)}"), 5.0);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn CEILING(?)}", 4.2), 5.0);
}

TEST_F(ScalarFunctionsTest, COS) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn COS(0)}"), 1.0);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn COS({fn PI()})}"), -1.0, 1e-10);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn COS(?)}", 0.0), 1.0);
}

TEST_F(ScalarFunctionsTest, COT) {
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn COT(({fn PI()} / 4))}"), 1.0, 1e-10);  // COT(45°) = 1
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn COT({fn PI()} / 4)}"), 1.0, 1e-10);  // COT(45°) = 1
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn COT({fn PI()} / 6)}"), 1.7320508075688772, 1e-10);  // COT(30°) = sqrt(3)
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn COT({fn PI()} / 3)}"), 0.5773502691896257, 1e-10);  // COT(60°) = 1/sqrt(3)
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn COT(1)}"), 0.6420926159343306, 1e-10);  // COT(1 radian)

    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn COT(?)}", 1.0), 0.6420926159343306, 1e-10);
}

TEST_F(ScalarFunctionsTest, DEGREES) {
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn DEGREES({fn PI()})}"), 180.0, 1e-10);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn DEGREES({fn PI()} / 2)}"), 90.0, 1e-10);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn DEGREES(0)}"), 0.0);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn DEGREES(?)}", 0.0), 0.0);
}

TEST_F(ScalarFunctionsTest, EXP) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn EXP(0)}"), 1.0);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn EXP(1)}"), 2.718281828459045, 1e-10);  // e

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn EXP(?)}", 0.0), 1.0);
}

TEST_F(ScalarFunctionsTest, FLOOR) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn FLOOR(4.7)}"), 4.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn FLOOR(-4.7)}"), -5.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn FLOOR(5.0)}"), 5.0);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn FLOOR(?)}", 4.7), 4.0);
}

TEST_F(ScalarFunctionsTest, LOG) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn LOG(1)}"), 0.0);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn LOG({fn EXP(1)})}"), 1.0, 1e-8);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn LOG(10)}"), 2.302585092994046, 1e-8);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn LOG(?)}", 1.0), 0.0);
}

TEST_F(ScalarFunctionsTest, LOG10) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn LOG10(1)}"), 0.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn LOG10(10)}"), 1.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn LOG10(100)}"), 2.0);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn LOG10(?)}", 10.0), 1.0);
}

TEST_F(ScalarFunctionsTest, MOD) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MOD(10, 3)}"), 1);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MOD(15, 5)}"), 0);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MOD(7, 2)}"), 1);

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MOD(?, ?)}", 10, 3), 1);
}

TEST_F(ScalarFunctionsTest, PI) {
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn PI()}"), 3.141592653589793, 1e-10);
}

TEST_F(ScalarFunctionsTest, POWER) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn POWER(2, 3)}"), 8.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn POWER(10, 2)}"), 100.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn POWER(4, 0.5)}"), 2.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn POWER(2, -1)}"), 0.5);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn POWER(?, ?)}", 2.0, 3.0), 8.0);
}

TEST_F(ScalarFunctionsTest, RADIANS) {
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn RADIANS(180)}"), 3.141592653589793, 1e-10);  // PI
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn RADIANS(90)}"), 1.5707963267948966, 1e-10);  // PI/2
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn RADIANS(0)}"), 0.0);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn RADIANS(?)}", 0.0), 0.0);
}

TEST_F(ScalarFunctionsTest, RAND) {
    // RAND returns value between 0 and 1
    auto result = query<SQLDOUBLE>("SELECT {fn RAND()}");
    ASSERT_GE(result, 0.0);
    ASSERT_LE(result, 1.0);

    // These cases are not implemented:
    // 1. Deterministic RAND with a seed
    // auto result1 = query<SQLDOUBLE>("SELECT {fn RAND(42)}");
    // auto result2 = query<SQLDOUBLE>("SELECT {fn RAND(42)}");
    // ASSERT_EQ(result1, result2);

    // 2. Rand must produce different values even when called in the same query
    // Currently it is not the case two calls to RAND() in the same query will produce the same
    // random value.
    // ASSERT_EQ(query<SQLINTEGER>("SELECT {fn RAND()} != {fn RAND()}"), 1);
}

TEST_F(ScalarFunctionsTest, ROUND) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ROUND(3.14159, 2)}"), 3.14);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ROUND(3.5)}"), 4.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ROUND(-3.5)}"), -4.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ROUND(1234.5678, -2)}"), 1200.0);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn ROUND(?, ?)}", 3.14159, 2), 3.14);
}

TEST_F(ScalarFunctionsTest, SIGN) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SIGN(42)}"), 1);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SIGN(-42)}"), -1);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SIGN(0)}"), 0);

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SIGN(?)}", 42), 1);
}

TEST_F(ScalarFunctionsTest, SIN) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn SIN(0)}"), 0.0);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn SIN({fn PI()} / 2)}"), 1.0, 1e-10);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn SIN(?)}", 0.0), 0.0);
}

TEST_F(ScalarFunctionsTest, SQRT) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn SQRT(4)}"), 2.0);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn SQRT(2)}"), 1.4142135623730951, 1e-10);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn SQRT(0)}"), 0.0);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn SQRT(?)}", SQLDOUBLE{4.0}), 2.0);
}

TEST_F(ScalarFunctionsTest, TAN) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TAN(0)}"), 0.0);
    ASSERT_NEAR(query<SQLDOUBLE>("SELECT {fn TAN({fn PI()} / 4)}"), 1.0, 1e-10);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TAN(?)}", 0.0), 0.0);
}

TEST_F(ScalarFunctionsTest, TRUNCATE) {
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TRUNCATE(3.14159, 2)}"), 3.14);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TRUNCATE(3.9)}"), 3.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TRUNCATE(-3.9)}"), -3.0);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TRUNCATE(1234.5678, -2)}"), 1200.0);

    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn TRUNCATE(?, ?)}", 3.14159, 2), 3.14);
}

// ============================================================================
// Date/Time Functions
// ============================================================================

TEST_F(ScalarFunctionsTest, CURRENT_DATE) {
    auto [res, year, month, day]
        = query<std::tuple<SQL_DATE_STRUCT, SQLINTEGER, SQLINTEGER, SQLINTEGER>>(
            "SELECT {fn CURRENT_DATE()}, year(today()), month(today()), day(today())"
        );
    ASSERT_EQ(res.year, year);
    ASSERT_EQ(res.month, month);
    ASSERT_EQ(res.day, day);
}

TEST_F(ScalarFunctionsTest, CURRENT_TIME) {

    auto [res, hour, minute, second]
        = query<std::tuple<SQL_TIME_STRUCT, SQLINTEGER, SQLINTEGER, SQLINTEGER>>(
            "SELECT {fn CURRENT_TIME()}, hour(now64(0)), minute(now64(0)), second(now64(0))"
        );
    ASSERT_EQ(res.hour, hour);
    ASSERT_EQ(res.minute, minute);
    ASSERT_EQ(res.second, second);
}

TEST_F(ScalarFunctionsTest, CURRENT_TIME_withoutParentheses) {

    auto [res, hour, minute, second]
        = query<std::tuple<SQL_TIME_STRUCT, SQLINTEGER, SQLINTEGER, SQLINTEGER>>(
            "SELECT {fn CURRENT_TIME}, hour(now64(0)), minute(now64(0)), second(now64(0))"
        );
    ASSERT_EQ(res.hour, hour);
    ASSERT_EQ(res.minute, minute);
    ASSERT_EQ(res.second, second);
}

TEST_F(ScalarFunctionsTest, CURRENT_TIMESTAMP) {
        auto [res, year, month, day, hour, minute, second, millisecond]
            = query<std::tuple<
                SQL_TIMESTAMP_STRUCT, SQLINTEGER, SQLINTEGER, SQLINTEGER,
                SQLINTEGER, SQLINTEGER, SQLINTEGER, SQLINTEGER>>(
                "SELECT {fn CURRENT_TIMESTAMP(3)}, year(now64(3)), month(now64(3)), day(now64(3)), "
                "hour(now64(3)), minute(now64(3)), second(now64(3)), millisecond(now64(3))");
        ASSERT_EQ(res.year, year);
        ASSERT_EQ(res.month, month);
        ASSERT_EQ(res.day, day);
        ASSERT_EQ(res.hour, hour);
        ASSERT_EQ(res.minute, minute);
        ASSERT_EQ(res.second, second);
        ASSERT_EQ(res.fraction, millisecond * 1'000'000);
}

TEST_F(ScalarFunctionsTest, CURRENT_TIMESTAMP_withoutParentheses) {
        auto [res, year, month, day, hour, minute, second]
            = query<std::tuple<
                SQL_TIMESTAMP_STRUCT, SQLINTEGER, SQLINTEGER, SQLINTEGER,
                SQLINTEGER, SQLINTEGER, SQLINTEGER>>(
                "SELECT {fn CURRENT_TIMESTAMP}, year(now64(3)), month(now64(3)), day(now64(3)), "
                "hour(now64(3)), minute(now64(3)), second(now64(3))");
        ASSERT_EQ(res.year, year);
        ASSERT_EQ(res.month, month);
        ASSERT_EQ(res.day, day);
        ASSERT_EQ(res.hour, hour);
        ASSERT_EQ(res.minute, minute);
        ASSERT_EQ(res.second, second);
}

TEST_F(ScalarFunctionsTest, NOW) {
    auto [res, year, month, day, hour, minute, second]
        = query<std::tuple<
            SQL_TIMESTAMP_STRUCT, SQLINTEGER, SQLINTEGER, SQLINTEGER,
            SQLINTEGER, SQLINTEGER, SQLINTEGER>>(
            "SELECT {fn NOW()}, year(now()), month(now()), day(now64()), "
            "hour(now64()), minute(now64()), second(now64())");
    ASSERT_EQ(res.year, year);
    ASSERT_EQ(res.month, month);
    ASSERT_EQ(res.day, day);
    ASSERT_EQ(res.hour, hour);
    ASSERT_EQ(res.minute, minute);
    ASSERT_EQ(res.second, second);
}

TEST_F(ScalarFunctionsTest, CURDATE) {
    auto [res, year, month, day]
        = query<std::tuple<SQL_DATE_STRUCT, SQLINTEGER, SQLINTEGER, SQLINTEGER>>(
            "SELECT {fn CURDATE()}, year(today()), month(today()), day(today())"
        );
    ASSERT_EQ(res.year, year);
    ASSERT_EQ(res.month, month);
    ASSERT_EQ(res.day, day);
}


TEST_F(ScalarFunctionsTest, CURTIME) {
    auto [res, hour, minute, second]
        = query<std::tuple<SQL_TIME_STRUCT, SQLINTEGER, SQLINTEGER, SQLINTEGER>>(
            "SELECT {fn CURTIME()}, hour(now64(0)), minute(now64(0)), second(now64(0))"
        );
    ASSERT_EQ(res.hour, hour);
    ASSERT_EQ(res.minute, minute);
    ASSERT_EQ(res.second, second);
}

TEST_F(ScalarFunctionsTest, DAYNAME) {
    // 2024-01-15 is a Monday
    ASSERT_EQ(query<std::string>("SELECT {fn DAYNAME({d '2024-01-15'})}"), "Monday");
    ASSERT_EQ(query<std::string>("SELECT {fn DAYNAME({d '2024-01-20'})}"), "Saturday");
    ASSERT_EQ(query<std::string>("SELECT {fn DAYNAME({d '2024-01-21'})}"), "Sunday");

    SQL_DATE_STRUCT date = {.year = 2024, .month = 1, .day = 21};
    ASSERT_EQ(query<std::string>("SELECT {fn DAYNAME(?)}", date), "Sunday");
}

TEST_F(ScalarFunctionsTest, DAYOFMONTH) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFMONTH({d '2024-01-15'})}"), 15);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFMONTH({d '2024-12-31'})}"), 31);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFMONTH({d '2024-02-29'})}"), 29);  // Leap year

    SQL_DATE_STRUCT date = {.year = 2024, .month = 1, .day = 15};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFMONTH(?)}", date), 15);
}

TEST_F(ScalarFunctionsTest, DAYOFWEEK) {
    // ODBC: 1=Sunday, 2=Monday, ..., 7=Saturday
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFWEEK({d '2024-01-14'})}"), 1);  // Sunday
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFWEEK({d '2024-01-15'})}"), 2);  // Monday
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFWEEK({d '2024-01-20'})}"), 7);  // Saturday

    SQL_DATE_STRUCT date = {.year = 2024, .month = 1, .day = 14};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFWEEK(?)}", date), 1);  // Sunday
}

TEST_F(ScalarFunctionsTest, DAYOFYEAR) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFYEAR({d '2024-01-01'})}"), 1);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFYEAR({d '2024-12-31'})}"), 366);  // Leap year
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFYEAR({d '2023-12-31'})}"), 365);  // Non-leap year

    SQL_DATE_STRUCT date = {.year = 2024, .month = 1, .day = 1};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn DAYOFYEAR(?)}", date), 1);
}

TEST_F(ScalarFunctionsTest, HOUR) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn HOUR({ts '2024-01-15 14:30:45'})}"), 14);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn HOUR({ts '2024-01-15 00:00:00'})}"), 0);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn HOUR({ts '2024-01-15 23:59:59'})}"), 23);

    SQL_TIMESTAMP_STRUCT ts = {.year = 2024, .month = 1, .day = 15, .hour = 14, .minute = 30, .second = 45};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn HOUR(?)}", ts), 14);
}

TEST_F(ScalarFunctionsTest, MINUTE) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MINUTE({ts '2024-01-15 14:30:45'})}"), 30);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MINUTE({ts '2024-01-15 00:00:00'})}"), 0);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MINUTE({ts '2024-01-15 23:59:59'})}"), 59);

    SQL_TIMESTAMP_STRUCT ts = {.year = 2024, .month = 1, .day = 15, .hour = 14, .minute = 30, .second = 45};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MINUTE(?)}", ts), 30);
}

TEST_F(ScalarFunctionsTest, MONTH) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MONTH({d '2024-01-15'})}"), 1);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MONTH({d '2024-06-15'})}"), 6);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MONTH({d '2024-12-31'})}"), 12);

    SQL_DATE_STRUCT date = {.year = 2024, .month = 6, .day = 15};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn MONTH(?)}", date), 6);
}

TEST_F(ScalarFunctionsTest, MONTHNAME) {
    ASSERT_EQ(query<std::string>("SELECT {fn MONTHNAME({d '2024-01-15'})}"), "January");
    ASSERT_EQ(query<std::string>("SELECT {fn MONTHNAME({d '2024-06-15'})}"), "June");
    ASSERT_EQ(query<std::string>("SELECT {fn MONTHNAME({d '2024-12-31'})}"), "December");

    SQL_DATE_STRUCT date = {.year = 2024, .month = 6, .day = 15};
    ASSERT_EQ(query<std::string>("SELECT {fn MONTHNAME(?)}", date), "June");
}

TEST_F(ScalarFunctionsTest, QUARTER) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn QUARTER({d '2024-01-15'})}"), 1);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn QUARTER({d '2024-04-15'})}"), 2);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn QUARTER({d '2024-07-15'})}"), 3);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn QUARTER({d '2024-10-15'})}"), 4);

    SQL_DATE_STRUCT date = {.year = 2024, .month = 7, .day = 15};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn QUARTER(?)}", date), 3);
}

TEST_F(ScalarFunctionsTest, SECOND) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SECOND({ts '2024-01-15 14:30:45'})}"), 45);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SECOND({ts '2024-01-15 00:00:00'})}"), 0);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SECOND({ts '2024-01-15 23:59:59'})}"), 59);

    SQL_TIMESTAMP_STRUCT ts = {.year = 2024, .month = 1, .day = 15, .hour = 14, .minute = 30, .second = 45};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn SECOND(?)}", ts), 45);
}

TEST_F(ScalarFunctionsTest, YEAR) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn YEAR({d '2024-01-15'})}"), 2024);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn YEAR({d '1999-12-31'})}"), 1999);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn YEAR({d '2000-01-01'})}"), 2000);

    SQL_DATE_STRUCT date = {.year = 2024, .month = 1, .day = 15};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn YEAR(?)}", date), 2024);
}

TEST_F(ScalarFunctionsTest, EXTRACT) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(YEAR FROM {d '2024-03-15'})}"), 2024);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(MONTH FROM {d '2024-03-15'})}"), 3);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(DAY FROM {d '2024-03-15'})}"), 15);

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(YEAR FROM {ts '2024-03-15 14:30:45'})}"), 2024);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(MONTH FROM {ts '2024-03-15 14:30:45'})}"), 3);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(DAY FROM {ts '2024-03-15 14:30:45'})}"), 15);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(HOUR FROM {ts '2024-03-15 14:30:45'})}"), 14);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(MINUTE FROM {ts '2024-03-15 14:30:45'})}"), 30);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(SECOND FROM {ts '2024-03-15 14:30:45'})}"), 45);

    SQL_DATE_STRUCT date = {.year = 2024, .month = 3, .day = 15};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(YEAR FROM ?)}", date), 2024);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(MONTH FROM ?)}", date), 3);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(DAY FROM ?)}", date), 15);

    SQL_TIMESTAMP_STRUCT ts = {.year = 2024, .month = 3, .day = 15, .hour = 14, .minute = 30, .second = 45};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(HOUR FROM ?)}", ts), 14);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(MINUTE FROM ?)}", ts), 30);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn EXTRACT(SECOND FROM ?)}", ts), 45);
}

TEST_F(ScalarFunctionsTest, TIMESTAMPADD) {
    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>(
        "SELECT {fn TIMESTAMPADD(SQL_TSI_SECOND, 30, {ts '2024-01-15 10:00:00'})}"),
        (SQL_TIMESTAMP_STRUCT{.year = 2024, .month = 1, .day = 15, .hour = 10, .minute = 0, .second = 30}));

    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>(
        "SELECT {fn TIMESTAMPADD(SQL_TSI_MINUTE, 45, {ts '2024-01-15 10:00:00'})}"),
        (SQL_TIMESTAMP_STRUCT{.year = 2024, .month = 1, .day = 15, .hour = 10, .minute = 45, .second = 0}));

    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>(
        "SELECT {fn TIMESTAMPADD(SQL_TSI_HOUR, 5, {ts '2024-01-15 10:00:00'})}"),
        (SQL_TIMESTAMP_STRUCT{.year = 2024, .month = 1, .day = 15, .hour = 15, .minute = 0, .second = 0}));

    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>(
        "SELECT {fn TIMESTAMPADD(SQL_TSI_DAY, 10, {ts '2024-01-15 10:00:00'})}"),
        (SQL_TIMESTAMP_STRUCT{.year = 2024, .month = 1, .day = 25, .hour = 10, .minute = 0, .second = 0}));

    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>(
        "SELECT {fn TIMESTAMPADD(SQL_TSI_WEEK, 2, {ts '2024-01-15 10:00:00'})}"),
        (SQL_TIMESTAMP_STRUCT{.year = 2024, .month = 1, .day = 29, .hour = 10, .minute = 0, .second = 0}));

    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>(
        "SELECT {fn TIMESTAMPADD(SQL_TSI_MONTH, 3, {ts '2024-01-15 10:00:00'})}"),
        (SQL_TIMESTAMP_STRUCT{.year = 2024, .month = 4, .day = 15, .hour = 10, .minute = 0, .second = 0}));

    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>(
        "SELECT {fn TIMESTAMPADD(SQL_TSI_QUARTER, 1, {ts '2024-01-15 10:00:00'})}"),
        (SQL_TIMESTAMP_STRUCT{.year = 2024, .month = 4, .day = 15, .hour = 10, .minute = 0, .second = 0}));

    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>(
        "SELECT {fn TIMESTAMPADD(SQL_TSI_YEAR, 2, {ts '2024-01-15 10:00:00'})}"),
        (SQL_TIMESTAMP_STRUCT{.year = 2026, .month = 1, .day = 15, .hour = 10, .minute = 0, .second = 0}));

    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>(
        "SELECT {fn TIMESTAMPADD(SQL_TSI_DAY, -5, {ts '2024-01-15 10:00:00'})}"),
        (SQL_TIMESTAMP_STRUCT{.year = 2024, .month = 1, .day = 10, .hour = 10, .minute = 0, .second = 0}));

    SQL_TIMESTAMP_STRUCT ts = {.year = 2024, .month = 1, .day = 15, .hour = 10, .minute = 0, .second = 0};
    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>(
        "SELECT {fn TIMESTAMPADD(SQL_TSI_DAY, 10, ?)}", ts),
        (SQL_TIMESTAMP_STRUCT{.year = 2024, .month = 1, .day = 25, .hour = 10, .minute = 0, .second = 0}));
}

TEST_F(ScalarFunctionsTest, TIMESTAMPDIFF) {
    ASSERT_EQ(query<SQLINTEGER>(
        "SELECT {fn TIMESTAMPDIFF(SQL_TSI_SECOND, {ts '2024-01-15 10:00:00'}, {ts '2024-01-15 10:00:30'})}"), 30);

    ASSERT_EQ(query<SQLINTEGER>(
        "SELECT {fn TIMESTAMPDIFF(SQL_TSI_MINUTE, {ts '2024-01-15 10:00:00'}, {ts '2024-01-15 10:45:00'})}"), 45);

    ASSERT_EQ(query<SQLINTEGER>(
        "SELECT {fn TIMESTAMPDIFF(SQL_TSI_HOUR, {ts '2024-01-15 10:00:00'}, {ts '2024-01-15 15:00:00'})}"), 5);

    ASSERT_EQ(query<SQLINTEGER>(
        "SELECT {fn TIMESTAMPDIFF(SQL_TSI_DAY, {ts '2024-01-15 10:00:00'}, {ts '2024-01-25 10:00:00'})}"), 10);

    ASSERT_EQ(query<SQLINTEGER>(
        "SELECT {fn TIMESTAMPDIFF(SQL_TSI_WEEK, {ts '2024-01-15 10:00:00'}, {ts '2024-01-29 10:00:00'})}"), 2);

    ASSERT_EQ(query<SQLINTEGER>(
        "SELECT {fn TIMESTAMPDIFF(SQL_TSI_MONTH, {ts '2024-01-15 10:00:00'}, {ts '2024-04-15 10:00:00'})}"), 3);

    ASSERT_EQ(query<SQLINTEGER>(
        "SELECT {fn TIMESTAMPDIFF(SQL_TSI_QUARTER, {ts '2024-01-15 10:00:00'}, {ts '2024-04-15 10:00:00'})}"), 1);

    ASSERT_EQ(query<SQLINTEGER>(
        "SELECT {fn TIMESTAMPDIFF(SQL_TSI_YEAR, {ts '2024-01-15 10:00:00'}, {ts '2026-01-15 10:00:00'})}"), 2);

    ASSERT_EQ(query<SQLINTEGER>(
        "SELECT {fn TIMESTAMPDIFF(SQL_TSI_DAY, {ts '2024-01-25 10:00:00'}, {ts '2024-01-15 10:00:00'})}"), -10);

    SQL_TIMESTAMP_STRUCT ts1 = {.year = 2024, .month = 1, .day = 15, .hour = 10, .minute = 0, .second = 0};
    SQL_TIMESTAMP_STRUCT ts2 = {.year = 2024, .month = 1, .day = 25, .hour = 10, .minute = 0, .second = 0};
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn TIMESTAMPDIFF(SQL_TSI_DAY, ?, ?)}", ts1, ts2), 10);
}

// ============================================================================
// System Functions
// ============================================================================

TEST_F(ScalarFunctionsTest, DATABASE) {
    auto [res, database] = query<std::tuple<std::string, std::string>>("SELECT {fn DATABASE()}, database()");
    ASSERT_EQ(res, database);
}

TEST_F(ScalarFunctionsTest, IFNULL) {
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn IFNULL(42, 0)}"), 42);
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn IFNULL(NULL, 99)}"), 99);

    ASSERT_EQ(query<std::string>("SELECT {fn IFNULL('hello', 'default')}"), "hello");
    ASSERT_EQ(query<std::string>("SELECT {fn IFNULL(NULL, 'default')}"), "default");

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn IFNULL(?, ?)}", 42, 0), 42);
    ASSERT_EQ(query<std::string>("SELECT {fn IFNULL(?, ?)}", "hello", "default"), "hello");
}

TEST_F(ScalarFunctionsTest, USER) {
    auto [res, user] = query<std::tuple<std::string, std::string>>("SELECT {fn USER()}, user()");
    ASSERT_EQ(res, user);
}

TEST_F(ScalarFunctionsTest, CONVERT) {
    // String to numeric conversions
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn CONVERT('123', SQL_INTEGER)}"), 123);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn CONVERT('3.14', SQL_DOUBLE)}"), 3.14);

    // Numeric to string conversions
    ASSERT_EQ(query<std::string>("SELECT {fn CONVERT(123, SQL_VARCHAR)}"), "123");

    // TODO(slabko): SQL_CHAR does not work
    // ASSERT_EQ(query<std::string>("SELECT {fn CONVERT(123, SQL_CHAR)}"), "123");

    // Numeric type conversions
    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn CONVERT(3.14, SQL_INTEGER)}"), 3);
    ASSERT_EQ(query<SQLDOUBLE>("SELECT {fn CONVERT(42, SQL_DOUBLE)}"), 42.0);

    // Date/time conversions
    ASSERT_EQ(query<SQL_DATE_STRUCT>("SELECT {fn CONVERT('2024-03-15', SQL_DATE)}"),
        (SQL_DATE_STRUCT{.year = 2024, .month = 3, .day = 15}));

    // TODO(slabko): SQL_TIME does not work
    // ASSERT_EQ(query<SQL_TIME_STRUCT>("SELECT {fn CONVERT('14:30:45', SQL_TIME)}"),
    //    (SQL_TIME_STRUCT{.hour = 14, .minute = 30, .second = 45}));

    ASSERT_EQ(query<SQL_TIMESTAMP_STRUCT>("SELECT {fn CONVERT('2024-03-15 14:30:45', SQL_TIMESTAMP)}"),
        (SQL_TIMESTAMP_STRUCT{.year = 2024, .month = 3, .day = 15, .hour = 14, .minute = 30, .second = 45}));

    // Timestamp to date
    ASSERT_EQ(query<SQL_DATE_STRUCT>("SELECT {fn CONVERT({ts '2024-03-15 14:30:45'}, SQL_DATE)}"),
        (SQL_DATE_STRUCT{.year = 2024, .month = 3, .day = 15}));

    // TODO(slabko): SQL_TIME does not work
    // Timestamp to time
    // ASSERT_EQ(query<SQL_TIME_STRUCT>("SELECT {fn CONVERT({ts '2024-03-15 14:30:45'}, SQL_TIME)}"),
    //    (SQL_TIME_STRUCT{.hour = 14, .minute = 30, .second = 45}));

    ASSERT_EQ(query<SQLINTEGER>("SELECT {fn CONVERT(?, SQL_INTEGER)}", "456"), 456);
    ASSERT_EQ(query<std::string>("SELECT {fn CONVERT(?, SQL_VARCHAR)}", 789), "789");
}
