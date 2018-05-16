-- sqlcmd -S .\SQLExpress -i mssql.linked.server.sql

EXEC master.dbo.sp_addlinkedserver
        @server = N'clickhouse_link_test'
       ,@srvproduct=N'Clickhouse'
       ,@provider=N'MSDASQL'
       ,@provstr=N'Driver={ClickHouse Unicode};SERVER=______server_name______;DATABASE=test;'

EXEC sp_serveroption  'clickhouse_link_test','rpc','true';
EXEC sp_serveroption 'clickhouse_link_test','rpc out','true';
EXEC('select name from system.databases;') at [clickhouse_link_test];
