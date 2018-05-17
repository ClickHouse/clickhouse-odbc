-- sqlcmd -S .\MSSQLSERVER -i mssql.linked.server.sql
-- sqlcmd -i mssql.linked.server.sql

EXEC master.dbo.sp_addlinkedserver
        @server = N'clickhouse_link_test'
       ,@srvproduct=N'Clickhouse'
       ,@provider=N'MSDASQL'
       ,@provstr=N'Driver={ClickHouse Unicode};SERVER=ch1.setun.net;DATABASE=test;'

EXEC sp_serveroption 'clickhouse_link_test','rpc','true';
EXEC sp_serveroption 'clickhouse_link_test','rpc out','true';
EXEC('select * from system.numbers limit 10;') at [clickhouse_link_test];
EXEC("select 'Just string'") at [clickhouse_link_test];
EXEC('select name from system.databases;') at [clickhouse_link_test];
