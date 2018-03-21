#include <escaping/escape_sequences.h>
#include <gtest/gtest.h>

TEST(EscapeSequencesCase, ParseConvert1) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONVERT(1, SQL_BIGINT)}"),
        "SELECT toInt64(1)"
    );
}

TEST(EscapeSequencesCase, ParseConvert2) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONVERT(-1.2, SQL_BIGINT)}"),
              "SELECT toInt64(-1.2)"
    );
}

TEST(EscapeSequencesCase, ParseConvert3) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT SUM({fn CONVERT(amount, SQL_BIGINT)})"),
        "SELECT SUM(toInt64(amount))"
    );
}

TEST(EscapeSequencesCase, ParseConvert4) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT SUM({fn CONVERT(Custom_SQL_Query.amount, SQL_BIGINT)})"),
              "SELECT SUM(toInt64(Custom_SQL_Query.amount))"
    );
}

TEST(EscapeSequencesCase, ParseConvert5) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT SUM({fn CONVERT(`Custom_SQL_Query`.`amount`, SQL_BIGINT)})"),
              "SELECT SUM(toInt64(`Custom_SQL_Query`.`amount`))"
    );
}

TEST(EscapeSequencesCase, ParseConvert6_0) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn ROUND(1.1 + 2.4, 1)}"),
              "SELECT round(1.1 + 2.4, 1)"
    );
}

TEST(EscapeSequencesCase, ParseConvert6) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONVERT({fn ROUND(1.1 + 2.4, 1)}, SQL_BIGINT)}"),
        "SELECT toInt64(round(1.1 + 2.4, 1))"
    );
}

TEST(EscapeSequencesCase, ParseConvert6_1) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT  {fn   CONVERT(  {fn   ROUND(  1.1  +  2.4  ,  1  )  }  ,  SQL_BIGINT  )  }"),
              "SELECT  toInt64(round(  1.1  +  2.4  ,  1  )  )"
    );
}


TEST(EscapeSequencesCase, ParseConcat) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONCAT('a', 'b')}"),
        "SELECT concat('a', 'b')"
    );

    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONCAT(`table`.`field1`, `table`.`field1`)}"),
        "SELECT concat(`table`.`field1`, `table`.`field1`)"
    );

    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONCAT({fn CONCAT(`table`.`field1`, '.')}, `table`.`field1`)}"),
        "SELECT concat(concat(`table`.`field1`, '.'), `table`.`field1`)"
    );
}

TEST(EscapeSequencesCase, ParseRound) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn ROUND(1.1 + 2.4, 1)}"),
        "SELECT round(1.1 + 2.4, 1)"
    );
}

TEST(EscapeSequencesCase, ParsePower) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn POWER(`f_g38d`.`hsf_thkd_wect_fxge`,2)}"),
        "SELECT pow(`f_g38d`.`hsf_thkd_wect_fxge`,2)"
    );
}

TEST(EscapeSequencesCase, ParseSqrt) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn SQRT(1 + 1)}"),
        "SELECT sqrt(1 + 1)"
    );
}

TEST(EscapeSequencesCase, ParseAbs) { ASSERT_EQ( replaceEscapeSequences("SELECT {fn ABS(1 + 1)}"), "SELECT abs(1 + 1)" ); }

TEST(EscapeSequencesCase, ParseAbsMinus) { ASSERT_EQ( replaceEscapeSequences("SELECT {fn ABS(-1 + -1)}"), "SELECT abs(-1 + -1)" ); }
TEST(EscapeSequencesCase, ParseAbsm1) { ASSERT_EQ( replaceEscapeSequences("SELECT {fn ABS(-1)}"), "SELECT abs(-1)" ); }

TEST(EscapeSequencesCase, ParseAbs2) { ASSERT_EQ( replaceEscapeSequences("SELECT COUNT({fn ABS(`test.odbc1`.`err_orr_arr`)})"), "SELECT COUNT(abs(`test.odbc1`.`err_orr_arr`))" ); }
TEST(EscapeSequencesCase, ParseAbs3) { ASSERT_EQ( replaceEscapeSequences("SELECT COUNT({fn ABS(`err_orr_arr`)})"), "SELECT COUNT(abs(`err_orr_arr`))" ); }
TEST(EscapeSequencesCase, ParseAbs4) { ASSERT_EQ( replaceEscapeSequences("SELECT COUNT({fn ABS(`test.odbc1`.`err_orr_arr`)}) AS `TEMP_Calculation_559572257702191122__2716881070__0_`"), "SELECT COUNT(abs(`test.odbc1`.`err_orr_arr`)) AS `TEMP_Calculation_559572257702191122__2716881070__0_`" ); }

TEST(EscapeSequencesCase, ParseTruncate) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT CAST({fn TRUNCATE(1.1 + 2.4, 1)} AS INTEGER) AS `yr_date_ok`"),
              "SELECT CAST(trunc(1.1 + 2.4, 1) AS INTEGER) AS `yr_date_ok`"
    );
    //ASSERT_EQ(
    //    replaceEscapeSequences("SELECT CAST({fn TRUNCATE(EXTRACT(YEAR FROM `Custom_SQL_Query`.`date`),0)} AS INTEGER) AS `yr_date_ok`"),
    //          "TODO: convert extract() function"
    //);
}

TEST(EscapeSequencesCase, ParseCurdate1) { ASSERT_EQ( replaceEscapeSequences("SELECT {fn CURDATE()}"), "SELECT today()" ); }


TEST(EscapeSequencesCase, ParseTimestampdiff2) { ASSERT_EQ( replaceEscapeSequences("SELECT {fn TIMESTAMPDIFF(SQL_TSI_DAY,CAST(`test`.`odbc1`.`datetime` AS DATE),{fn CURDATE()} )}"), "SELECT dateDiff('day',CAST(`test`.`odbc1`.`datetime` AS DATE),today() )"
); }

TEST(EscapeSequencesCase, Parsetimestampdiff) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn TIMESTAMPDIFF(SQL_TSI_DAY,CAST(`activity`.`min_activation_yabrowser` AS DATE),CAST(`activity`.`date` AS DATE))} AS `Calculation_503558746242125826`, SUM({fn CONVERT(1, SQL_BIGINT)}) AS `sum_Number_of_Records_ok`"),
              "SELECT dateDiff('day',CAST(`activity`.`min_activation_yabrowser` AS DATE),CAST(`activity`.`date` AS DATE)) AS `Calculation_503558746242125826`, SUM(toInt64(1)) AS `sum_Number_of_Records_ok`"
    );
}

TEST(EscapeSequencesCase, ParseTimestampadd1) { ASSERT_EQ( replaceEscapeSequences("SELECT {fn TIMESTAMPADD(SQL_TSI_YEAR, 1, {fn CURDATE()})}"),
"SELECT addYears(today(), 1)"
); }

TEST(EscapeSequencesCase, ParseTimestampadd2) { ASSERT_EQ( replaceEscapeSequences("SELECT {fn  TIMESTAMPADD(  SQL_TSI_YEAR  ,  1  ,  {fn  CURDATE()  }  )  }"),
                                                           "SELECT addYears(today()  , 1)"
); }

TEST(EscapeSequencesCase, ParseTimestampadd3) { ASSERT_EQ( replaceEscapeSequences("SELECT {fn TIMESTAMPADD(SQL_TSI_DAY,1,CAST({fn CURRENT_TIMESTAMP(0)} AS DATE))}"),
                                                           "SELECT addDays(CAST(toUnixTimestamp(now()) AS DATE), 1)"
); }

TEST(EscapeSequencesCase, ParseTimestampadd4) { ASSERT_EQ( replaceEscapeSequences("SELECT {fn TIMESTAMPADD( SQL_TSI_DAY , 1 , CAST( {fn CURRENT_TIMESTAMP( 0 ) }  AS  DATE ) ) } "),
                                                           "SELECT addDays(CAST(toUnixTimestamp(now())  AS  DATE ), 1) "
); }

TEST(EscapeSequencesCase, ParseCurrentTimestamp1) { ASSERT_EQ( replaceEscapeSequences("SELECT {fn CURRENT_TIMESTAMP}"),
                                                           "SELECT toUnixTimestamp(now())"
); }
TEST(EscapeSequencesCase, ParseCurrentTimestamp2) { ASSERT_EQ( replaceEscapeSequences("SELECT  {fn  CURRENT_TIMESTAMP } "),
                                                           "SELECT  toUnixTimestamp(now()) "
); }



TEST(EscapeSequencesCase, DateTime) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {d '2017-01-01'}"),
        "SELECT toDate('2017-01-01')"
    );

    ASSERT_EQ(
        replaceEscapeSequences("SELECT {ts '2017-01-01 10:01:01'}"),
        "SELECT toDateTime('2017-01-01 10:01:01')"
    );

    // We cutting off milliseconds from timestamp because CH server
    // doesn't support them.
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {ts '2017-01-01 10:01:01.555'}"),
        "SELECT toDateTime('2017-01-01 10:01:01')"
    );
    // Strange date format. Symbols after last dot shouldn't be cutted off.
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {ts '2017.01.01 10:01:01'}"),
        "SELECT toDateTime('2017.01.01 10:01:01')"
    );
}
