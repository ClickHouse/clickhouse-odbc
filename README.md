## Cloning a Project with Submodules

Please, note - [git submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) are used in this project. 

When you clone such a project, by default you get the directories that contain submodules, but none of the files within them.
So, in order to build the project, you need either:
  * clone repo with all submodules altogether (use `--recursive`)
```bash
git clone --recursive https://github.com/yandex/clickhouse-odbc
```
  * or add submodules manually after main project cloned - in the root of source tree run:
```bash
git submodule init && git submodule update
```

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
