[![Build Status](https://travis-ci.org/clickhouse/clickhouse-odbc.svg?branch=master)](https://travis-ci.org/clickhouse/clickhouse-odbc)

If you are macos user see [MACOS.md](MACOS.md)

## Cloning a Project with Submodules

Please note - [git submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) are used in this project. 

When you clone such a project, by default you get the directories that contain submodules, but none of the files within them.
So, in order to build the project, you need either:
  * clone repo with all submodules altogether (use `--recursive`)
```bash
git clone --recursive https://github.com/clickhouse/clickhouse-odbc
```
  * or add submodules manually after main project cloned - in the root of source tree run:
```bash
git submodule update --init --recursive
```

## Installing Prerequisites (Linux)

You'll need to have installed:
  * Fresh C compiler, which understands -std=c++14
  * Static libraries 
    * static **libgcc**
    * static **libstdc++**
    * static **libodbc**
  * cmake >= 3


### DEB-based Linux
Install unixodbc-dev >= 2.3.0 or libiodbc2-dev
```bash
sudo apt install unixodbc-dev
```
or
```bash
sudo apt install libiodbc2-dev
```

### RPM-based Linux
CentOS is shipped with gcc 4.X, which is not suitable for this task.
Fedora and CentOS do not have static libodbc provided, so you'll need either to build your own, or download 3-rd party packages.
Static libodbc is available for [Fedora 25](https://github.com/Altinity/unixODBC/tree/master/RPMS/Fedora25) and [Fedora 26](https://github.com/Altinity/unixODBC/tree/master/RPMS/Fedora26).
If you are running another OS, you can try to build your own RPMs from [this project](https://github.com/Altinity/unixODBC).


## Building (Linux)

1. At the root of source directory:
```bash
mkdir -p build; cd build && cmake .. && make -j $(nproc || sysctl -n hw.ncpu || echo 4)
```
Please use cmake3 to build the project on CentOS 7. You can install it with `yum install cmake3`.

2. libclickhouseodbc.so will be at ```build/driver/libclickhouseodbc.so```

```bash
mkdir -p build; cd build && cmake .. && make -j $(nproc || sysctl -n hw.ncpu || echo 4)
```

## Building (Linux Debian based .deb package)
```bash
sudo apt install -y devscripts debhelper cmake ninja-build lsb-release unixodbc-dev
debuild -us -uc -i --source-option=--format="3.0 (native)"
```
or
```bash
sudo apt install -y sudo pbuilder fakeroot debhelper debian-archive-keyring debian-keyring
sudo pbuilder create --configfile debian/.pbuilderrc && pdebuild --configfile debian/.pbuilderrc
```


## Building (windows visual studio)
```bat
cd vs && build_vs.bat
```

## Building (windows cmake) (Developer only: setup window still not working)
```bat
cd vs && build_cmake.bat
```

## Build with tests (needs configured ~/.odbc.ini with DSN=clickhouse_localhost)
```bash
mkdir -p build; cd build
( cd ../contrib && git clone https://github.com/nanodbc/nanodbc )
cmake -G Ninja -DTEST_DSN=clickhouse_localhost -DCMAKE_BUILD_TYPE=Debug -DUSE_DEBUG_17=1 .. && ninja
ctest -V
```

## ODBC configuration

Edit ~/.odbc.ini :

```ini
[ODBC Data Sources]
Clickhouse = Clickhouse

[ClickHouse]
Driver = $(PATH_OF_CLICKHOUSE_ODBC_SO)
# Optional settings:
#Description = ClickHouse driver
# New all-in one way to specify connection with [optional] settings:
#url = https://default:password@localhost:8443/query?database=default&max_result_bytes=4000000&buffer_size=3000000
# Minimal (will connect to port 8443 if https:// or 8123 if http:// ):
url = https://localhost

# Old way:
#server = localhost
#password = 123456
#database = default
#uid = default
#port = 8123

# sslmode variants: allow - ignore self-signed and bad certificates; require - check certificates (and fail connection if something wrong)
#sslmode = require
#privatekeyfile =
#certificatefile =
#calocation =

# Timeout for http queries to ClickHouse server (default is 30 seconds)
#timeout=60

#trace=1
#tracefile=/tmp/clickhouse-odbc.log
```

Sometimes you should change ~/.odbcinst.ini or /etc/odbcinst.ini or /Library/ODBC/odbcinst.ini :
```ini
[ODBC Drivers]
Clickhouse = Installed

[Clickhouse]
Driver=$(PATH_OF_CLICKHOUSE_ODBC_SO)
```

## Testing
Run `isql -v ClickHouse`

Also look [test](test)/ contents
