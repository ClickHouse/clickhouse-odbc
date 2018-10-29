## Simple install (macos brew):

Install brew:
```brew --help &>/dev/null || /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"```

If you already have installed unixodbc - unlink it:
```brew unlink unixodbc```

If you want to connect to new clickhouse server (versions 18.10.3 and later):
```
brew install https://raw.githubusercontent.com/proller/homebrew-core/chodbc/Formula/clickhouse-odbc.rb
```

If you want to connect to old clickhouse server (versions before 18.10.3):
```
brew install https://raw.githubusercontent.com/proller/homebrew-core/chodbcold/Formula/clickhouse-odbc.rb

```
If you want to switch new/old versions - use ```brew reinstall``` instead of ```brew install```


edit ```~/.odbc.ini``` :
```(ini)
[ClickHouse]
Driver = /usr/local/opt/clickhouse-odbc/lib/libclickhouseodbcw.dylib
# Optional settings:
#server = localhost
#password = 123456
#port = 8123
#database = default
#uid = default
#sslmode = require
```



## Advanced install (for developers):

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
#password = 123456
#database = default
#uid = default
#port = 8123
#sslmode = require
```
