#!/usr/bin/env bash

# this test requires running clickhouse server and configured ~/.odbc.ini
# cp -n /usr/share/doc/clickhouse-odbc/examples/odbc.ini ~/.odbc.ini
# apt install unixodbc

# to build and install package:
# cd .. && debuild -us -uc -i --source-option=--format="3.0 (native)" && sudo dpkg -i `ls ../clickhouse-odbc_*_amd64.deb | tail -n1`

# test https:
# cd .. && debuild -eDH_VERBOSE=1 -eCMAKE_FLAGS="-DFORCE_STATIC_LINK=" -us -uc -i --source-option=--format="3.0 (native)" && sudo dpkg -i `ls ../clickhouse-odbc_*_amd64.deb | tail -n1`

# Should not have any errors:
# ./test.sh | grep -i error

DSN=${DSN=clickhouse_localhost}
[ -z $RUNNER ] && RUNNER=`which isql` && [ -n $RUNNER ] && RUNNER_PARAMS0="-v -b"
[ -z $RUNNER ] && RUNNER=`which iusql` && [ -n $RUNNER ] && RUNNER_PARAMS0="-v -b"
[ -z $RUNNER ] && RUNNER=`which iodbctestw`
[ -z $RUNNER ] && RUNNER=`which iodbctest`

function q {
    echo "Asking [$*]"
    # DYLD_INSERT_LIBRARIES=/usr/local/opt/gcc/lib/gcc/8/libasan.5.dylib
    # export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libasan.so.5
    echo "$*" | $RUNNER $DSN $RUNNER_PARAMS0 $RUNNER_PARAMS
}

echo "Using: $RUNNER $DSN $RUNNER_PARAMS0 $RUNNER_PARAMS"

q "SELECT * FROM system.build_options;"
q "CREATE DATABASE IF NOT EXISTS test;"
q "DROP TABLE IF EXISTS test.odbc1;"
q "CREATE TABLE test.odbc1 (ui64 UInt64, string String, date Date, datetime DateTime) ENGINE = Memory;"
q "INSERT INTO test.odbc1 VALUES (1, '2', 3, 4);"
q "INSERT INTO test.odbc1 VALUES (10, '20', 30, 40);"
q "INSERT INTO test.odbc1 VALUES (100, '200', 300, 400);"
q "SELECT * FROM test.odbc1 WHERE ui64=1;"

q 'SELECT {fn CONVERT(1, SQL_BIGINT)}'
q "SELECT {fn CONVERT(100000, SQL_TINYINT)}"
q "SELECT {fn CONCAT('a', 'b')}"
q 'SELECT CAST({fn TRUNCATE(1.1 + 2.4, 1)} AS INTEGER) AS `yr_date_ok`'

q $'SELECT COUNT({fn ABS(`test`.`odbc1`.`ui64`)}) FROM test.odbc1'

q $'SELECT {fn TIMESTAMPDIFF(SQL_TSI_DAY,CAST(`test`.`odbc1`.`datetime` AS DATE),CAST(`test`.`odbc1`.`date` AS DATE))} AS `Calculation_503558746242125826`, SUM({fn CONVERT(1, SQL_BIGINT)}) AS `sum_Number_of_Records_ok` FROM `test`.`odbc1` WHERE (CAST(`test`.`odbc1`.`datetime` AS DATE) <> {d \'1970-01-01\'}) GROUP BY `Calculation_503558746242125826`'

q $'SELECT COUNT({fn ABS(`test`.`odbc1`.`ui64`)}) AS `TEMP_Calculation_559572257702191122__2716881070__0_`, SUM({fn ABS(`test`.`odbc1`.`ui64`)}) AS `TEMP_Calculation_559572257702191122__3054398615__0_`  FROM test.odbc1;'

q $'SELECT SUM((CASE WHEN (`test`.`odbc1`.`ui64` * `test`.`odbc1`.`ui64`) < 0 THEN NULL ELSE {fn SQRT((`test`.`odbc1`.`ui64` * `test`.`odbc1`.`ui64`))} END)) AS `TEMP_Calculation_559572257701634065__1464080195__0_`, COUNT((CASE WHEN (`test`.`odbc1`.`ui64` * `test`.`odbc1`.`ui64`) < 0 THEN NULL ELSE {fn SQRT((`test`.`odbc1`.`ui64` * `test`.`odbc1`.`ui64`))} END)) AS `TEMP_Calculation_559572257701634065__2225718044__0_` FROM test.odbc1;'

# SELECT (CASE WHEN (NOT = 'True') OR (`test`.`odbc1`.`string` = 'True') OR (`test`.`odbc1`.`string2` = 'True') THEN 1 WHEN NOT (NOT = 'True') OR (`test`.`odbc1`.`string` = 'True') OR (`test`.`odbc1`.`string` = 'True') OR (`test`.`odbc1`.`string2` = 'True') THEN 0 ELSE NULL END) AS `Calculation_597289912116125696`,
# SUM({fn CONVERT(1, SQL_BIGINT)}) AS `sum_Number_of_Records_ok` FROM `test`.`odbc1` GROUP BY `Calculation_597289912116125696`, `string`, `ui64`

q "DROP TABLE IF EXISTS test.purchase_stat;"
q "CREATE TABLE test.purchase_stat (purchase_id UInt64, purchase_date DateTime, offer_category UInt64, amount UInt64) ENGINE = Memory;"
q $'SELECT SUM({fn CONVERT(Custom_SQL_Query.amount, SQL_BIGINT)}) AS sum_amount FROM (SELECT purchase_date, offer_category, SUM(amount) AS amount, COUNT(DISTINCT purchase_id) AS purchase_id FROM test.purchase_stat WHERE (offer_category = 1) GROUP BY purchase_date, offer_category) Custom_SQL_Query HAVING (COUNT(1) > 0)'
q $'SELECT (CASE WHEN (`test`.`odbc1`.`ui64` > 0) THEN 1 WHEN NOT (`test`.`odbc1`.`ui64` > 0) THEN 0 ELSE NULL END) AS `Calculation_162692564973015040`, SUM({fn CONVERT(1, SQL_BIGINT)}) AS `sum_Number_of_Records_ok` FROM `test`.`odbc1` GROUP BY (CASE WHEN (`test`.`odbc1`.`ui64` > 0) THEN 1 WHEN NOT (`test`.`odbc1`.`ui64` > 0) THEN 0 ELSE NULL END)'
q $"SELECT {d '2017-08-30'}"
q 'SELECT CAST(CAST(`odbc1`.`date` AS DATE) AS DATE) AS `tdy_Calculation_687361904651595777_ok` FROM `test`.`odbc1`'

q 'SELECT {fn CURDATE()}'

q $'SELECT `test`.`odbc1`.`ui64` AS `bannerid`, SUM((CASE WHEN `test`.`odbc1`.`ui64` = 0 THEN NULL ELSE `test`.`odbc1`.`ui64` / `test`.`odbc1`.`ui64` END)) AS `sum_Calculation_582934706662502402_ok`, SUM(`test`.`odbc1`.`ui64`) AS `sum_clicks_ok`, SUM(`test`.`odbc1`.`ui64`) AS `sum_shows_ok`, SUM(`test`.`odbc1`.`ui64`) AS `sum_true_installs_ok`, CAST(CAST(`test`.`odbc1`.`date` AS DATE) AS DATE) AS `tdy_Calculation_582934706642255872_ok` FROM `test`.`odbc1` WHERE (`test`.`odbc1`.`string` = \'YandexBrowser\') GROUP BY `test`.`odbc1`.`ui64`, CAST(CAST(`test`.`odbc1`.`date` AS DATE) AS DATE)'


q $'SELECT test.odbc1.ui64 AS BannerID,   SUM((CASE WHEN test.odbc1.ui64 = 0 THEN NULL ELSE test.odbc1.ui64 / test.odbc1.ui64 END)) AS sum_Calculation_500744014152380416_ok,   SUM(test.odbc1.ui64) AS sum_ch_installs_ok,   SUM(test.odbc1.ui64) AS sum_goodshows_ok FROM test.odbc1 GROUP BY test.odbc1.ui64'
q $'SELECT test.odbc1.ui64 AS BannerID,   SUM((CASE WHEN test.odbc1.ui64 > 0 THEN NULL ELSE test.odbc1.ui64 / test.odbc1.ui64 END)) AS sum_Calculation_500744014152380416_ok,   SUM(test.odbc1.ui64) AS sum_ch_installs_ok,   SUM(test.odbc1.ui64) AS sum_goodshows_ok FROM test.odbc1 GROUP BY test.odbc1.ui64'


q "DROP TABLE IF EXISTS test.test_tableau;"
q "create table test.test_tableau (country String, clicks UInt64, shows UInt64) engine Log"
q "insert into test.test_tableau values ('ru',10000,100500),('ua',1000,6000),('by',2000,6500),('tr',100,500)"
q "insert into test.test_tableau values ('undefined',0,2)"
q "insert into test.test_tableau values ('injected',1,0)"
q 'SELECT test.test_tableau.country AS country, SUM((CASE WHEN test.test_tableau.shows = 0 THEN NULL ELSE CAST(test.test_tableau.clicks AS FLOAT) / test.test_tableau.shows END)) AS sum_Calculation_920986154656493569_ok, SUM({fn POWER(CAST(test.test_tableau.clicks AS FLOAT),2)}) AS sum_Calculation_920986154656579587_ok FROM test.test_tableau GROUP BY test.test_tableau.country;'
q "DROP TABLE test.test_tableau;"

q 'SELECT NULL'
q 'SELECT [NULL]'

q "DROP TABLE IF EXISTS test.adv_watch;"
q "create table test.adv_watch (rocket_date Date, rocket_datetime dateTime, ivi_id UInt64) engine Log"
q "insert into test.adv_watch values (1,2,3)"
q "insert into test.adv_watch values (1, {fn TIMESTAMPADD(SQL_TSI_DAY,-8,CAST({fn CURRENT_TIMESTAMP(0)} AS DATE))}, 3)"
q 'SELECT `test`.`adv_watch`.`rocket_date` AS `rocket_date`, COUNT(DISTINCT `test`.`adv_watch`.`ivi_id`) AS `usr_Calculation_683139814283419648_ok` FROM `test`.`adv_watch` WHERE ((`adv_watch`.`rocket_datetime` >= {fn TIMESTAMPADD(SQL_TSI_DAY,-9,CAST({fn CURRENT_TIMESTAMP(0)} AS DATE))}) AND (`test`.`adv_watch`.`rocket_datetime` < {fn TIMESTAMPADD(SQL_TSI_DAY,1,CAST({fn CURRENT_TIMESTAMP(0)} AS DATE))})) GROUP BY `test`.`adv_watch`.`rocket_date`'
q 'SELECT CAST({fn TRUNCATE(EXTRACT(YEAR FROM `test`.`adv_watch`.`rocket_date`),0)} AS INTEGER) AS `yr_rocket_date_ok` FROM `test`.`adv_watch` GROUP BY CAST({fn TRUNCATE(EXTRACT(YEAR FROM `test`.`adv_watch`.`rocket_date`),0)} AS INTEGER)'
q "DROP TABLE test.adv_watch;"

# https://github.com/yandex/clickhouse-odbc/issues/43
q 'DROP TABLE IF EXISTS test.gamoraparams;'
q 'CREATE TABLE test.gamoraparams ( user_id Int64, date Date, dt DateTime, p1 Nullable(Int32), platforms Nullable(Int32), max_position Nullable(Int32), vv Nullable(Int32), city Nullable(String), third_party Nullable(Int8), mobile_tablet Nullable(Int8), mobile_phone Nullable(Int8), desktop Nullable(Int8), web_mobile Nullable(Int8), tv_attach Nullable(Int8), smart_tv Nullable(Int8), subsite_id Nullable(Int32), view_in_second Nullable(Int32), view_in_second_presto Nullable(Int32)) ENGINE = MergeTree(date, user_id, 8192)'
q 'insert into test.gamoraparams values (1, {fn CURRENT_TIMESTAMP }, CAST({fn CURRENT_TIMESTAMP(0)} AS DATE), Null, Null,Null,Null,Null, Null,Null,Null,Null,Null,Null,Null,Null,Null,Null);'
q 'SELECT `Custom_SQL_Query`.`platforms` AS `platforms` FROM (select platforms from test.gamoraparams where platforms is null limit 1) `Custom_SQL_Query` GROUP BY `platforms`'
q 'SELECT CAST({fn TRUNCATE(EXTRACT(YEAR FROM `test`.`gamoraparams`.`dt`),0)} AS INTEGER) AS `yr_date_ok` FROM `test`.`gamoraparams` GROUP BY `yr_date_ok`';
q 'DROP TABLE test.gamoraparams;'

q $'SELECT CAST(EXTRACT(YEAR FROM `odbc1`.`date`) AS INTEGER) AS `yr_date_ok` FROM `test`.`odbc1`'
q $'SELECT CAST({fn TRUNCATE(EXTRACT(YEAR FROM `odbc1`.`date`),0)} AS INTEGER) AS `yr_date_ok` FROM `test`.`odbc1`'
q $'SELECT SUM({fn CONVERT(1, SQL_BIGINT)}) AS `sum_Number_of_Records_ok`, CAST({fn TRUNCATE(EXTRACT(YEAR FROM `odbc1`.`date`),0)} AS INTEGER) AS `yr_date_ok` FROM `test`.`odbc1` GROUP BY CAST({fn TRUNCATE(EXTRACT(YEAR FROM `odbc1`.`date`),0)} AS INTEGER)'

q 'SELECT CAST({fn TRUNCATE(EXTRACT(YEAR FROM CAST(`test`.`odbc1`.`date` AS DATE)),0)} AS INTEGER) AS `yr_Calculation_860750537261912064_ok` FROM `test`.`odbc1` GROUP BY `yr_Calculation_860750537261912064_ok`'
q 'SELECT {fn TIMESTAMPADD(SQL_TSI_DAY,CAST({fn TRUNCATE((-1 * ({fn DAYOFYEAR(`test`.`odbc1`.`date`)} - 1)),0)} AS INTEGER),CAST(`test`.`odbc1`.`date` AS DATE))} AS `tyr__date_ok` FROM `test`.`odbc1` GROUP BY `tyr__date_ok`'

q 'SELECT {fn TIMESTAMPADD(SQL_TSI_DAY,(-1 * ({fn MOD((7 + {fn DAYOFWEEK(CAST(`test`.`odbc1`.`date` AS DATE))} - 2), 7)})),CAST(CAST(`test`.`odbc1`.`date` AS DATE) AS DATE))} AS `twk_date_ok` FROM `test`.`odbc1` GROUP BY `twk_date_ok`'
q 'SELECT {fn TIMESTAMPADD(SQL_TSI_DAY,CAST({fn TRUNCATE((-1 * ({fn DAYOFYEAR(CAST(`test`.`odbc1`.`date` AS DATE))} - 1)),0)} AS INTEGER),CAST(CAST(`test`.`odbc1`.`date` AS DATE) AS DATE))} AS `tyr_Calculation_681450978608578560_ok` FROM `test`.`odbc1` GROUP BY `tyr_Calculation_681450978608578560_ok`'
q 'SELECT {fn TIMESTAMPADD(SQL_TSI_MONTH,CAST({fn TRUNCATE((3 * (CAST({fn TRUNCATE({fn QUARTER(CAST(`test`.`odbc1`.`date` AS DATE))},0)} AS INTEGER) - 1)),0)} AS INTEGER),{fn TIMESTAMPADD(SQL_TSI_DAY,CAST({fn TRUNCATE((-1 * ({fn DAYOFYEAR(CAST(`test`.`odbc1`.`date` AS DATE))} - 1)),0)} AS INTEGER),CAST(CAST(`test`.`odbc1`.`date` AS DATE) AS DATE))})} AS `tqr_Calculation_681450978608578560_ok` FROM `test`.`odbc1` GROUP BY `tqr_Calculation_681450978608578560_ok`'
q 'SELECT {fn TIMESTAMPADD(SQL_TSI_DAY,CAST({fn TRUNCATE((-1 * (EXTRACT(DAY FROM CAST(`test`.`odbc1`.`date` AS DATE)) - 1)),0)} AS INTEGER),CAST(CAST(`test`.`odbc1`.`date` AS DATE) AS DATE))} AS `tmn_Calculation_681450978608578560_ok` FROM `test`.`odbc1` GROUP BY `tmn_Calculation_681450978608578560_ok`'

q $'SELECT (CASE WHEN (`test`.`odbc1`.`ui64` < 5) THEN replaceRegexpOne(toString(`test`.`odbc1`.`ui64`), \'^\\s+\', \'\') WHEN (`test`.`odbc1`.`ui64` < 10) THEN \'5-9\' WHEN (`test`.`odbc1`.`ui64` < 20) THEN \'10-19\' WHEN (`test`.`odbc1`.`ui64` >= 20) THEN \'20+\' ELSE NULL END) AS `Calculation_582653228063055875`, SUM(`test`.`odbc1`.`ui64`) AS `sum_traf_se_ok` FROM `test`.`odbc1` GROUP BY `Calculation_582653228063055875` ORDER BY `Calculation_582653228063055875`'
q $"SELECT *, (CASE WHEN (number == 1) THEN 'o' WHEN (number == 2) THEN 'two long string' WHEN (number == 3) THEN 'r' WHEN (number == 4) THEN NULL ELSE '-' END)  FROM system.numbers LIMIT 6"

# todo: test with fail on comparsion:
q $"SELECT {fn DAYOFWEEK(CAST('2018-04-16' AS DATE))}, 7, 'sat'"
q $"SELECT {fn DAYOFWEEK(CAST('2018-04-15' AS DATE))}, 1, 'sun'"
q $"SELECT {fn DAYOFWEEK(CAST('2018-04-16' AS DATE))}, 2, 'mon'"
q $"SELECT {fn DAYOFWEEK(CAST('2018-04-17' AS DATE))}, 3, 'thu'"
q $"SELECT {fn DAYOFWEEK(CAST('2018-04-18' AS DATE))}, 4, 'wed'"
q $"SELECT {fn DAYOFWEEK(CAST('2018-04-19' AS DATE))}, 5, 'thu'"
q $"SELECT {fn DAYOFWEEK(CAST('2018-04-20' AS DATE))}, 6, 'fri'"
q $"SELECT {fn DAYOFWEEK(CAST('2018-04-21' AS DATE))}, 7, 'sat'"
q $"SELECT {fn DAYOFWEEK(CAST('2018-04-22' AS DATE))}, 1, 'sun'"

q $"SELECT {fn DAYOFYEAR(CAST('2018-01-01' AS DATE))}, 1"
q $"SELECT {fn DAYOFYEAR(CAST('2018-04-20' AS DATE))}, 110"
q $"SELECT {fn DAYOFYEAR(CAST('2018-12-31' AS DATE))}, 365"

q $'SELECT name, {fn REPLACE(`name`, \'E\',\'!\')} AS `r1` FROM system.build_options'
q $'SELECT {fn REPLACE(\'ABCDABCD\' , \'B\',\'E\')} AS `r1`'

q $"SELECT (CASE WHEN 1>0 THEN 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' ELSE NULL END);"
q $'SELECT {fn REPLACE(\'ABCDEFGHIJKLMNOPQRSTUVWXYZ\', \'E\',\'!\')} AS `r1`'

q $"SELECT 'абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ'"

q "SELECT toNullable(42), toNullable('abc'), NULL"
q "SELECT 1, 'string', NULL"
q "SELECT 1, NULL, 2, 3, NULL, 4"
q "SELECT 'stringlong', NULL, 2, NULL"

q $"SELECT -127,-128,-129,126,127,128,255,256,257,-32767,-32768,-32769,32766,32767,32768,65535,65536,65537,-2147483647,-2147483648,-2147483649,2147483646,2147483647,2147483648,4294967295,4294967296,4294967297,-9223372036854775807,-9223372036854775808,-9223372036854775809,9223372036854775806,9223372036854775807,9223372036854775808,18446744073709551615,18446744073709551616,18446744073709551617"
q $"SELECT 2147483647, 2147483648, 2147483647+1, 2147483647+10, 4294967295"


q "CREATE TABLE IF NOT EXISTS test.fixedstring ( xx FixedString(100)) ENGINE = Memory;"
q "INSERT INTO test.fixedstring VALUES ('a'), ('abcdefg'), ('абвгдеёжзийклмнопрстуфхцч')";
q "select xx as x from test.fixedstring;"
q "DROP TABLE test.fixedstring;"


q 'DROP TABLE IF EXISTS test.increment;'
q 'CREATE TABLE test.increment (n UInt64) engine Log;'

NUM=${NUM=100}
for i in `seq 1 ${NUM}`; do
    q "insert into test.increment values ($i);" > /dev/null
    q 'select * from test.increment;' > /dev/null
done

q 'select * from test.increment;'

echo "should be ${NUM}:"
q 'select count(*) from test.increment;'

q 'DROP TABLE test.increment;'


q "DROP TABLE IF EXISTS test.decimal;"
q "CREATE TABLE IF NOT EXISTS test.decimal (a DECIMAL(9,0), b DECIMAL(18,0), c DECIMAL(38,0), d DECIMAL(9, 9), e Decimal64(18), f Decimal128(38), g Decimal32(5), h Decimal64(9), i Decimal128(18), j dec(4,2)) ENGINE = Memory;"
q "INSERT INTO test.decimal (a, b, c, d, e, f, g, h, i, j) VALUES (42, 42, 42, 0.42, 0.42, 0.42, 42.42, 42.42, 42.42, 42.42);"
q "INSERT INTO test.decimal (a, b, c, d, e, f, g, h, i, j) VALUES (-42, -42, -42, -0.42, -0.42, -0.42, -42.42, -42.42, -42.42, -42.42);"
q "SELECT * FROM test.decimal;"

q "drop table if exists test.lc;"
q "create table test.lc (b LowCardinality(String)) engine=MergeTree order by b;"
q "insert into test.lc select '0123456789' from numbers(100);"
q "select count(), b from test.lc group by b;"
q "select * from test.lc limit 10;"
q "drop table test.lc;"


# q "SELECT number, toString(number), toDate(number) FROM system.numbers LIMIT 10000;"

echo "\n\n\nLast log:\n"
# cat /tmp/clickhouse-odbc-stderr.$USER
tail -n200 /tmp/clickhouse-odbc.log
