-- net stop MSSQLSERVER && net start MSSQLSERVER
-- sqlcmd -i mssql.linked.server.sql

EXEC master.dbo.sp_dropserver N'clickhouse_link_test';
EXEC master.dbo.sp_addlinkedserver
        @server = N'clickhouse_link_test'
       ,@srvproduct=N'Clickhouse'
       ,@provider=N'MSDASQL'
       ,@provstr=N'Driver={ClickHouse Unicode};SERVER=your.server.name.com;DATABASE=system;stringmaxlength=8000;'

EXEC sp_serveroption 'clickhouse_link_test','rpc','true';
EXEC sp_serveroption 'clickhouse_link_test','rpc out','true';
EXEC('select * from system.numbers limit 10;') at [clickhouse_link_test];
EXEC("select 'Just string'") at [clickhouse_link_test];
EXEC('select name from system.databases;') at [clickhouse_link_test];
EXEC('select * from system.build_options;') at [clickhouse_link_test];


exec('CREATE TABLE IF NOT EXISTS test.fixedstring ( xx FixedString(100)) ENGINE = TinyLog;') at [clickhouse_link_test];
exec(N'INSERT INTO test.fixedstring VALUES (''a''), (''abcdefg''), (''абвгдеёжзийклмнопрст'');') at [clickhouse_link_test];
--exec('INSERT INTO test.fixedstring VALUES (''a''),(''abcdefg'');') at [clickhouse_link_test];
exec('select xx as x from test.fixedstring;') at [clickhouse_link_test];
exec('DROP TABLE test.fixedstring;') at [clickhouse_link_test];
