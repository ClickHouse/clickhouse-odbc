
    // https://docs.faircom.com/doc/sqlref/33391.htm
    // https://docs.microsoft.com/en-us/sql/odbc/reference/appendixes/appendix-e-scalar-functions

    // Numeric
    DECLARE2(ABS, "abs"),
    DECLARE2(ACOS, "acos"),
    DECLARE2(ASIN, "asin"),
    DECLARE2(ATAN, "atan"),
    // DECLARE2(ATAN2, ""),
    DECLARE2(CEILING, "ceil"),
    DECLARE2(COS, "cos"),
    // DECLARE2(COT, ""),
    // DECLARE2(DEGREES, ""),
    DECLARE2(EXP, "exp"),
    DECLARE2(FLOOR, "floor"),
    DECLARE2(LOG, "log"),
    DECLARE2(LOG10, "log10"),
    DECLARE2(MOD, "modulo"),
    DECLARE2(PI, "pi"),
    DECLARE2(POWER, "pow"),
    // DECLARE2(RADIANS, ""),
    DECLARE2(RAND, "rand"),
    DECLARE2(ROUND, "round"),
    // DECLARE2(SIGN, ""),
    DECLARE2(SIN, "sin"),
    DECLARE2(SQRT, "sqrt"),
    DECLARE2(TAN, "tan"),
    DECLARE2(TRUNCATE, "trunc"),

    // String
    DECLARE2(CONCAT, "concat"),
    DECLARE2(LCASE, "lower"),
    DECLARE2(REPLACE, "replaceAll"),
    // TODO.

    // Date
    DECLARE2(CURDATE, "today"),
    DECLARE2(CURRENT_DATE, "today"),
    DECLARE2(DAYOFMONTH, "toDayOfMonth"),
    //DECLARE2(DAYOFWEEK, " toDayOfWeek"), // special handling
    //DECLARE2(DAYOFYEAR, " toDayOfYear"), // Supported by ClickHouse since 18.13.0
    DECLARE2(EXTRACT, "EXTRACT"), // Do not touch extract inside {fn ... }
    DECLARE2(HOUR, "toHour"),
    DECLARE2(MINUTE, "toMinute"),
    DECLARE2(MONTH, "toMonth"),
    DECLARE2(NOW, "now"),
    DECLARE2(SECOND, "toSecond"),
    DECLARE2(TIMESTAMPDIFF, "dateDiff"),
    DECLARE2(WEEK, "toDayOfWeek"),
    DECLARE2(SQL_TSI_QUARTER, "toQuarter"),
    DECLARE2(YEAR, "toYear"),

    // TODO.

    // TODO.
