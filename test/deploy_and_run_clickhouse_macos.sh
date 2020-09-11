#!/usr/bin/env bash

# Later better native wrappers should appear...

set -Eeo pipefail

echo macOS ClickHouse binary installation

MY_PATH=`dirname "$0"`
MY_PATH=`( cd "$MY_PATH" && pwd )`

rm -rf clickhouse
mkdir -p clickhouse
pushd clickhouse

echo 1. Prepare folders
mkdir -p usr/bin etc/clickhouse-server var/lib/clickhouse

echo 2. Download binaries
# TODO: switch to actual 20.3 binaries once available. Currently, this is master branch.
curl https://builds.clickhouse.tech/master/macos/clickhouse -o usr/bin/clickhouse
curl https://builds.clickhouse.tech/master/macos/clickhouse-odbc-bridge -o usr/bin/clickhouse-odbc-bridge

echo 3. Download configs
# TODO: switch to the corresponding versions of configs when the binaries are fixed. Currently, this is master branch.
curl https://raw.githubusercontent.com/ClickHouse/ClickHouse/master/programs/server/config.xml -o etc/clickhouse-server/config.xml
curl https://raw.githubusercontent.com/ClickHouse/ClickHouse/master/programs/server/users.xml -o etc/clickhouse-server/users.xml

echo 4. Setup executables
pushd usr/bin/
ln -s clickhouse clickhouse-client
ln -s clickhouse clickhouse-server
ln -s clickhouse clickhouse-extract-from-config
ln -s clickhouse clickhouse-benchmark
ln -s clickhouse clickhouse-performance-test
ln -s clickhouse clickhouse-compressor
ln -s clickhouse clickhouse-copier
ln -s clickhouse clickhouse-obfuscator
ln -s clickhouse clickhouse-format
chmod +x clickhouse*
popd

echo 5. Start the server in background
$MY_PATH/run_clickhouse_macos.sh # will run in background
# $MY_PATH/run_clickhouse_macos.sh foreground # to run in foreground

popd
