## Building (Linux)

1. Install unixodbc >= 2.3.0
```bash
sudo apt install unixodbc-dev
```

2. At the root of source directory:
```bash
mkdir -p build; cd build && cmake .. && make -j $(nproc || sysctl -n hw.ncpu || echo 2)
```

3. clickhouse-odbc.so will be at ```build/driver/clickhouse-odbc.so```

## ODBC configuration

vim ~/.odbc.ini:

```(ini)
[ClickHouse]
Driver = $(PATH_OF_CLICKHOUSE_ODBC_SO)
Description = ClickHouse driver
DATABASE = default
SERVER = localhost
PORT = 8123
FRAMED = 0
```

## Testing
Run ```isql -v ClickHouse```
