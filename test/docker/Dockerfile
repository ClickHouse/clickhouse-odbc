# syntax=docker/dockerfile:1.3-labs
# image to build & test clickhouse-odbc on ubuntu with docker

# How to build
# $ docker buildx build . -f ./test/docker/Dockerfile
#
# How to run
# $ #start ch server somehow either locally, with docker or use remote machine (requires chaning CH_SERVER_URL)
# $ docker run --network=host -e CH_SERVER_URL=http://localhost clickhouse_odbc_tester:test -R 'test.sh-isql-dsn-0'

# unixodbc or iodbc
ARG ODBC_PROVIDER=unixodbc

FROM ubuntu:20.04 as clickhouse_odbc_tester_base

RUN apt-get update -y \
    && DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC \
    apt install -y \
        build-essential \
        ninja-build \
        cmake \
        perl \
        python-is-python3 \
        python3 \
        python3-pip \
        python3-setuptools \
        libpoco-dev \
        libssl-dev \
        libicu-dev

RUN pip3 install --user \
    'testflows==1.6.56'


FROM clickhouse_odbc_tester_base as clickhouse_odbc_tester_base_unixodbc
RUN apt install -y unixodbc unixodbc-dev
ENV CMAKE_ODBC_PROVIDER=UnixODBC

FROM clickhouse_odbc_tester_base as clickhouse_odbc_tester_base_iodbc
RUN apt install -y iodbc libiodbc2 libiodbc2-dev
ENV CMAKE_ODBC_PROVIDER=iODBC

FROM clickhouse_odbc_tester_base_${ODBC_PROVIDER} as final
ENV CH_SERVER_URL=http://localhost
RUN echo ${CMAKE_ODBC_PROVIDER}

# There is no sane way to exclude ./test directory (that could change a lot)
# so copying everything required for build in multiple steps to maximize build caching
COPY CMakeLists.txt CPackLists.txt /clickhouse-odbc/
COPY ./cmake /clickhouse-odbc/cmake
COPY ./contrib /clickhouse-odbc/contrib
COPY ./driver /clickhouse-odbc/driver
COPY ./packaging /clickhouse-odbc/packaging
COPY ./test/CMakeLists.txt /clickhouse-odbc/test/CMakeLists.txt

ENV LOG_DIR=/var/log/ch-odbc
ARG BIN_DIR=/clickhouse-odbc-build

# We need to install it after odbc provider, since we need a 'sql.h' to build it
RUN pip3 install --user \
    'pyodbc>=4.0.0'

RUN mkdir -p ${BIN_DIR} \
    && echo $CMAKE_ODBC_PROVIDER \
    && cd ${BIN_DIR} \
    && cmake -G Ninja -DODBC_PROVIDER=${CMAKE_ODBC_PROVIDER} -DTEST_DSN_LIST="clickhouse_localhost;clickhouse_localhost_w" /clickhouse-odbc/ \
    && ninja

RUN ln -s ${BIN_DIR}/driver/libclickhouseodbc.so /usr/local/lib/libclickhouseodbc.so \
    && ln -s ${BIN_DIR}/driver/libclickhouseodbcw.so /usr/local/lib/libclickhouseodbcw.so

# Copy it last since it can be changed frequently
COPY ./test /clickhouse-odbc/test

# put the test into docker-entrypoint-initdb.d to run the tests on container initialization
# it just was the simplest
RUN cat <<EOF > /entrypoint.sh
#!/bin/bash

set -euo pipefail

mkdir -p \${LOG_DIR}
cd "${BIN_DIR}"

echo ctest args: \$@

# To allow running container against different ch servers,
# creating config file at runtime.
# Configurable with
#  - CH_SERVER_URL - url to connect to CH server
#  - LOG_DIR - path to directory with logs
#
cat << BASH_EOF > ~/.odbc.ini
[clickhouse_localhost]
Driver=${BIN_DIR}/driver/libclickhouseodbc.so
url=\${CH_SERVER_URL}
Trace     = 1
TraceFile = \${LOG_DIR}/clickhouse_localhost-trace.log
Debug     = 1
DebugFile = \${LOG_DIR}/clickhouse_localhost-debug.log

[clickhouse_localhost_w]
Driver=${BIN_DIR}/driver/libclickhouseodbcw.so
url=\${CH_SERVER_URL}
Trace     = 1
TraceFile = \${LOG_DIR}/clickhouse_w_localhost-trace.log
Debug     = 1
DebugFile = \${LOG_DIR}/clickhouse_w_localhost-debug.log
BASH_EOF

echo 'some ODBC info'
odbcinst -j
echo

echo 'config:'
cat  ~/.odbc.ini

echo 'starting tests:'
ctest -V --output-on-failure \$@

echo printing out log files from "${LOG_DIR}"
ls -lah ${LOG_DIR}
for log_file in ${LOG_DIR}/*.log; do
    echo ${log_file} ;
    cat ${log_file} ;
done

exit -1

EOF

ENTRYPOINT [ "/bin/bash", "-vx", "/entrypoint.sh" ]
CMD [ "-E", ".*-ut.*" ]

RUN chmod +x /entrypoint.sh