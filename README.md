## Building

1. Install unixodbc >= 2.3.0
2. Download Poco source
3. Run ```./configure --static --minimal --no-tests --cflags=-fPIC```
4. Run ```make && make install```
5. Run ```./build.sh```

## ODBC configuration

vim ~/.odbc.ini:

```(ini)
[ClickHouse]
Driver = /home/milovidov/work/ClickHouse/dbms/src/ODBC/odbc.so
Description = ClickHouse driver
DATABASE = default
HOST = localhost
PORT = 8123
FRAMED = 0
```

## Testing
Run ```iusql -v ClickHouse```
