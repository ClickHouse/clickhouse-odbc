## Building (macos brew):
```brew install https://raw.githubusercontent.com/proller/homebrew-core/chodbc/Formula/clickhouse-odbc.rb```

edit ```~/.odbc.ini``` :
```(ini)
[ClickHouse]
Driver = /usr/local/opt/clickhouse-odbc/lib/libclickhouseodbcw.dylib
# Optional settings:
#server = localhost
#database = default
#uid = default
#port = 8123
#sslmode = require
```



## Building (macos manual):

Download and prepare:
```bash
brew install git cmake
git clone --recursive https://github.com/yandex/clickhouse-odbc
cd clickhouse-odbc
```

Before build with standard libiodbc:
```
brew install libiodbc
```
Or for build with unixodbc:
```
brew install unixodbc
```

Build:
```
mkdir -p build; cd build && cmake .. && make -j $(nproc || sysctl -n hw.ncpu || echo 4)
```

edit ~/.odbc.ini:

```(ini)
[ClickHouse]
Driver = /Users/YOUR_USER_NAME/clickhouse-odbc/build/driver/libclickhouseodbcw.so
# Optional settings:
#Description = ClickHouse driver
#server = localhost
#database = default
#uid = default
#port = 8123
#sslmode = require
```
