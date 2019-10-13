# later better native wrappers should appear

mkdir clickhouse

pushd clickhouse
echo MacOS ClickHouse binary installation 

echo 1. Prepare folders.
mkdir -p usr/bin etc/clickhouse-server var/lib/clickhouse
echo 2. Download binaries
curl https://clickhouse-builds.s3.yandex.net/0/381947509a4f66236f943beaefb0b1f5c2fd979d/1570028580_binary/clickhouse -o usr/bin/clickhouse
curl https://clickhouse-builds.s3.yandex.net/0/381947509a4f66236f943beaefb0b1f5c2fd979d/1570028580_binary/clickhouse-odbc-bridge -o usr/bin/clickhouse-odbc-bridge
echo 3. Download configs
curl https://raw.githubusercontent.com/ClickHouse/ClickHouse/master/dbms/programs/server/config.xml -o etc/clickhouse-server/config.xml
curl https://raw.githubusercontent.com/ClickHouse/ClickHouse/master/dbms/programs/server/users.xml -o etc/clickhouse-server/users.xml
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
echo 4. Download script 
curl https://gist.githubusercontent.com/filimonov/2536ecd82baf184534f7f1d8e76a5f1e/raw/run_here.sh -o run_here.sh
chmod +x run_here.sh

echo starting server...

./run_here.sh # will run in background
popd

# ./run_here.sh foreground # to run in foreground 

# usr/bin/clickhouse-client
# kill $(pgrep clickhouse-server)
