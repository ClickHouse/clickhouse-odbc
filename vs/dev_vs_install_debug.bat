
:: installer64\bin\Debug\clickhouse_odbc_x64.msi /quiet

copy Debug\*.dll "C:\Program Files (x86)\ClickHouse ODBC"
copy x64\Debug\*.dll "C:\Program Files\ClickHouse ODBC"
