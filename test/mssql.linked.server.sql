-- net stop MSSQLSERVER && net start MSSQLSERVER
-- sqlcmd -i mssql.linked.server.sql

EXEC master.dbo.sp_dropserver N'clickhouse_link_test';
EXEC master.dbo.sp_addlinkedserver
        @server = N'clickhouse_link_test'
       ,@srvproduct=N'Clickhouse'
       ,@provider=N'MSDASQL'
       ,@provstr=N'Driver={ClickHouse Unicode};SERVER=!!!!!!!your.server.name.com!!!!!!!;DATABASE=system;stringmaxlength=8000;'

EXEC sp_serveroption 'clickhouse_link_test','rpc','true';
EXEC sp_serveroption 'clickhouse_link_test','rpc out','true';
EXEC('select * from system.numbers limit 10;') at [clickhouse_link_test];
EXEC("select 'Just string'") at [clickhouse_link_test];
EXEC('select name from system.databases;') at [clickhouse_link_test];
EXEC('select * from system.build_options;') at [clickhouse_link_test];


exec('CREATE TABLE IF NOT EXISTS test.fixedstring ( xx FixedString(100)) ENGINE = Memory;') at [clickhouse_link_test];
exec(N'INSERT INTO test.fixedstring VALUES (''a''), (''abcdefg''), (''абвгдеёжзийклмнопрстуфх'');') at [clickhouse_link_test];
--exec('INSERT INTO test.fixedstring VALUES (''a''),(''abcdefg'');') at [clickhouse_link_test];
exec('select xx as x from test.fixedstring;') at [clickhouse_link_test];
exec('DROP TABLE test.fixedstring;') at [clickhouse_link_test];

exec('SELECT -127,-128,-129,126,127,128,255,256,257,-32767,-32768,-32769,32766,32767,32768,65535,65536,65537,-2147483647,-2147483648,-2147483649,2147483646,2147483647,2147483648,4294967295,4294967296,4294967297,-9223372036854775807,-9223372036854775808,-9223372036854775809,9223372036854775806,9223372036854775807,9223372036854775808,18446744073709551615,18446744073709551616,18446744073709551617;') at [clickhouse_link_test];
exec('SELECT *, (CASE WHEN (number == 1) THEN ''o'' WHEN (number == 2) THEN ''two long string'' WHEN (number == 3) THEN ''r'' WHEN (number == 4) THEN NULL ELSE ''-'' END)  FROM system.numbers LIMIT 6') at [clickhouse_link_test];
