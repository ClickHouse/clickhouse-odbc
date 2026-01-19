
// https://docs.faircom.com/doc/sqlref/33391.htm
// https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/appendix-e-scalar-functions

// clang-format off

    // String
    DECLARE2(ASCII, "ascii"),
    DECLARE2(BIT_LENGTH, ""), // special handling
    DECLARE2(CHAR, "char"),
    DECLARE2(CHAR_LENGTH, "lengthUTF8"),
    DECLARE2(CHARACTER_LENGTH, "lengthUTF8"),
    DECLARE2(CONCAT, "concat"),
    DECLARE2(DIFFERENCE, ""), // special handling
    DECLARE2(INSERT, ""), // special handling
    DECLARE2(LCASE, "lowerUTF8"),
    DECLARE2(LEFT, "leftUTF8"),
    DECLARE2(LENGTH, "lengthUTF8"),
    DECLARE2(LOCATE, "" /* "position" */), // special handling
    DECLARE2(LTRIM, ""), // special handling
    DECLARE2(OCTET_LENGTH, "length"),
    DECLARE2(POSITION, ""), // special handling
    DECLARE2(REPEAT, "repeat"),
    DECLARE2(REPLACE, "replaceAll"),
    DECLARE2(RIGHT, "rightUTF8"),
    DECLARE2(RTRIM, "trimRight"),
    DECLARE2(SOUNDEX, "soundex"),
    DECLARE2(SPACE, ""), // special handling
    DECLARE2(SUBSTRING, "substringUTF8"),
    DECLARE2(UCASE, "upperUTF8"),

    // Non-standard scalar functions
    DECLARE2(LOWER, "lowerUTF8"),
    DECLARE2(UPPER, "upperUTF8"),

    // Numeric
    DECLARE2(ABS, "abs"),
    DECLARE2(ACOS, "acos"),
    DECLARE2(ASIN, "asin"),
    DECLARE2(ATAN, "atan"),
    DECLARE2(ATAN2, "atan2"),          //  ATAN2( float_exp1, float_exp2) 
    DECLARE2(CEILING, "ceil"),
    DECLARE2(COS, "cos"),
    // DECLARE2(COT, ""),              //  COT( float_exp )  // (1.0 / tan(float_expr))
    DECLARE2(DEGREES, "degrees"),
    DECLARE2(EXP, "exp"),
    DECLARE2(FLOOR, "floor"),
    DECLARE2(LOG, "log"),
    DECLARE2(LOG10, "log10"),
    DECLARE2(MOD, "modulo"),
    DECLARE2(PI, "pi"),
    DECLARE2(POWER, "pow"),
    DECLARE2(RADIANS, "radians"),
    DECLARE2(RAND, "randCanonical"),
    DECLARE2(ROUND, "round"),
    DECLARE2(SIGN, "sign"),
    DECLARE2(SIN, "sin"),
    DECLARE2(SQRT, "sqrt"),
    DECLARE2(TAN, "tan"),
    DECLARE2(TRUNCATE, "trunc"),



    // Date
    DECLARE2(CURRENT_DATE, "today"),
    DECLARE2(CURRENT_TIME, ""),      // now64, but allows calling without parentheses
    DECLARE2(CURRENT_TIMESTAMP, ""), // now64, but allows calling without parentheses
    DECLARE2(CURDATE, "today"),
    DECLARE2(CURTIME, "now64"),
    DECLARE2(DAYNAME, ""), // dateName('weekday', <value>)

    DECLARE2(DAYOFMONTH, "toDayOfMonth"),
    DECLARE2(DAYOFWEEK, "" /* "toDayOfWeek" */), // special handling
    DECLARE2(DAYOFYEAR, " toDayOfYear"), // Supported by ClickHouse since 18.13.0
    DECLARE2(EXTRACT, "EXTRACT"),
    DECLARE2(HOUR, "toHour"),
    DECLARE2(MINUTE, "toMinute"),
    DECLARE2(MONTH, "toMonth"),
    DECLARE2(MONTHNAME, ""), // dateName('weekday', <value>)
    DECLARE2(NOW, "now"),
    DECLARE2(SECOND, "toSecond"),
    DECLARE2(TIMESTAMPADD, ""), // special handling
    DECLARE2(TIMESTAMPDIFF, "dateDiff"),
    DECLARE2(WEEK, "toISOWeek"),
    DECLARE2(QUARTER, "toQuarter"),
    DECLARE2(YEAR, "toYear"),

    DECLARE2(DATABASE, "database"),
    DECLARE2(IFNULL, "ifNull"),
    DECLARE2(USER, "user"),

    // Conversion
    DECLARE2(CONVERT, ""), // special handling
