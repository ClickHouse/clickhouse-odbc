# syntax=docker/dockerfile:1.3-labs
# image to build & test clickhouse-odbc on ubuntu with docker

# How to build
# $ docker buildx build . -f ./test/docker/Dockerfile --build-arg ODBC_PROVIDER=unixodbc -t clickhouse_odbc_tester:test
#
# How to run
# $ #start ch server somehow either locally, with docker or use remote machine (requires chaning CH_SERVER_URL)
# $ docker run --network=host -e CH_SERVER_URL=http://localhost clickhouse_odbc_tester:test -R 'test.sh-isql-dsn-0'

# unixodbc or iodbc, note that values are lowercase due to docker's limitation on image names.
ARG ODBC_PROVIDER=unixodbc
# or centos
ARG BASE_OS=ubuntu

FROM ubuntu:22.04 as clickhouse_odbc_tester_base_ubuntu

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
        libicu-dev \
        clang \
        llvm \
        lldb

RUN pip3 install --user \
    'testflows==1.6.56'

#
#FROM centos:7 as clickhouse_odbc_tester_base_centos
#RUN yum makecache
## required for ninja-build
#RUN yum install -y epel-release
#RUN TZ=Etc/UTC \
#    yum install -y \
#        gcc \
#        gcc-c++ \
#        make \
#        ninja-build \
#        perl \
#        python3 \
#        python3-pip \
#        python3-devel \
#        python3-setuptools \
#        openssl-devel \
#        libicu \
#        libicu-devel
#
## project requires cmake that is newer than installable with yum
#ADD https://github.com/Kitware/CMake/releases/download/v3.20.6/cmake-3.20.6-linux-x86_64.sh .
#RUN chmod +x cmake-3.20.6-linux-x86_64.sh \
#    && ./cmake-3.20.6-linux-x86_64.sh --skip-license --prefix=/usr/local \
#    && rm ./cmake-3.20.6-linux-x86_64.sh \
#    && cmake --version
#
## install C++17 compatitable compiler
#ADD http://mirror.centos.org/centos/7/sclo/x86_64/rh/Packages/d/devtoolset-8-gcc-c++-8.3.1-3.2.el7.x86_64.rpm .
#ADD http://mirror.centos.org/centos/7/sclo/x86_64/rh/Packages/d/devtoolset-8-libstdc++-devel-8.3.1-3.2.el7.x86_64.rpm .
#ADD http://mirror.centos.org/centos/7/sclo/x86_64/rh/Packages/d/devtoolset-8-runtime-8.1-1.el7.x86_64.rpm .
#ADD http://mirror.centos.org/centos/7/sclo/x86_64/rh/Packages/d/devtoolset-8-gcc-8.3.1-3.2.el7.x86_64.rpm .
#ADD http://mirror.centos.org/centos/7/sclo/x86_64/rh/Packages/d/devtoolset-8-binutils-2.30-55.el7.2.x86_64.rpm .
#RUN ls -lah ./*.rpm \
#    && yum install -y ./*.rpm
#ENV CC=/opt/rh/devtoolset-8/root/usr/bin/gcc
#ENV CXX=/opt/rh/devtoolset-8/root/usr/bin/g++
#ENV CMAKE_ADDITIONAL_FLAGS="-DCH_ODBC_RUNTIME_LINK_STATIC=ON"
#
#RUN pip3 install \
#    'testflows==1.6.56'

FROM clickhouse_odbc_tester_base_ubuntu as clickhouse_odbc_tester_base_unixodbc_ubuntu
RUN apt install -y unixodbc unixodbc-dev
ENV CMAKE_ODBC_PROVIDER=UnixODBC

FROM clickhouse_odbc_tester_base_ubuntu as clickhouse_odbc_tester_base_iodbc_ubuntu
RUN apt install -y iodbc libiodbc2 libiodbc2-dev
ENV CMAKE_ODBC_PROVIDER=iODBC


#FROM clickhouse_odbc_tester_base_centos as clickhouse_odbc_tester_base_unixodbc_centos
#RUN yum install -y unixODBC-devel unixODBC
#ENV CMAKE_ODBC_PROVIDER=UnixODBC
#
#FROM clickhouse_odbc_tester_base_centos as clickhouse_odbc_tester_base_iodbc_centos
#RUN yum install -y libiodbc libiodbc-devel
#ENV CMAKE_ODBC_PROVIDER=iODBC


FROM clickhouse_odbc_tester_base_${ODBC_PROVIDER}_${BASE_OS} as final
ENV CH_SERVER_URL=http://localhost

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
    && cmake \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
        -G Ninja \
        -DODBC_PROVIDER=${CMAKE_ODBC_PROVIDER} \
        -DTEST_DSN_LIST="ClickHouse DSN (ANSI);ClickHouse DSN (Unicode);ClickHouse DSN (ANSI, RBWNAT)" \
        ${CMAKE_ADDITIONAL_FLAGS} \
        /clickhouse-odbc/ \
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

export ODBCSYSINI=~
export ODBCINSTINI=.odbcinst.ini
export ODBCINI=\$ODBCSYSINI/.odbc.ini
if [[ "\${CMAKE_ODBC_PROVIDER}" == "iODBC" ]]; then
    # Full path to a custom odbcinst.ini in ODBCINSTINI for iODBC.
    export ODBCINSTINI=$ODBCSYSINI/$ODBCINSTINI
fi

cat > \$ODBCSYSINI/.odbcinst.ini <<-BASH_EOF
[ODBC]
Trace     = 1
TraceFile = \${LOG_DIR}/odbc-driver-manager-trace.log
Debug     = 1
DebugFile = \${LOG_DIR}/odbc-driver-manager-debug.log

[ODBC Drivers]
ClickHouse ODBC Driver (ANSI)    = Installed
ClickHouse ODBC Driver (Unicode) = Installed

[ClickHouse ODBC Driver (ANSI)]
Driver     = ${BIN_DIR}/driver/libclickhouseodbc.so
Setup      = ${BIN_DIR}/driver/libclickhouseodbc.so
UsageCount = 1

[ClickHouse ODBC Driver (Unicode)]
Driver     = ${BIN_DIR}/driver/libclickhouseodbcw.so
Setup      = ${BIN_DIR}/driver/libclickhouseodbcw.so
UsageCount = 1
BASH_EOF

cat > \$ODBCSYSINI/.odbc.ini <<-BASH_EOF
[ODBC]
Trace     = 1
TraceFile = \${LOG_DIR}/odbc-driver-manager-trace.log
Debug     = 1
DebugFile = \${LOG_DIR}/odbc-driver-manager-debug.log

[ODBC Data Sources]
ClickHouse DSN (ANSI)         = ClickHouse ODBC Driver (ANSI)
ClickHouse DSN (Unicode)      = ClickHouse ODBC Driver (Unicode)
ClickHouse DSN (ANSI, RBWNAT) = ClickHouse ODBC Driver (ANSI)

[ClickHouse DSN (ANSI)]
Driver        = ClickHouse ODBC Driver (ANSI)
Description   = Test DSN for ClickHouse ODBC Driver (ANSI)
Url           = \${CH_SERVER_URL}
DriverLog     = yes
DriverLogFile = \${LOG_DIR}/clickhouse-odbc-driver.log

[ClickHouse DSN (Unicode)]
Driver        = ClickHouse ODBC Driver (Unicode)
Description   = Test DSN for ClickHouse ODBC Driver (Unicode)
Url           = \${CH_SERVER_URL}
DriverLog     = yes
DriverLogFile = \${LOG_DIR}/clickhouse-odbc-driver-w.log

[ClickHouse DSN (ANSI, RBWNAT)]
Driver        = ClickHouse ODBC Driver (ANSI)
Description   = Test DSN for ClickHouse ODBC Driver (ANSI) that uses RowBinaryWithNamesAndTypes as data source communication default format
Url           = \${CH_SERVER_URL}/query?default_format=RowBinaryWithNamesAndTypes
DriverLog     = yes
DriverLogFile = \${LOG_DIR}/clickhouse-odbc-driver.log
BASH_EOF

echo 'some ODBC info'
odbcinst -j
echo

echo 'config ~/.odbc.ini:'
cat  ~/.odbc.ini

echo 'config ~/.odbcinst.ini:'
cat  ~/.odbcinst.ini

echo 'starting tests:'
ctest -V --output-on-failure \$@

echo printing out log files from "\${LOG_DIR}"
ls -lah \${LOG_DIR}
# shopt -s nullglob
# for log_file in \${LOG_DIR}/*.log; do
#     echo \${log_file} ;
#     cat \${log_file} ;
# done

exit 0

EOF

ENTRYPOINT [ "/bin/bash", "-vx", "/entrypoint.sh" ]
CMD [ "-E", ".*-ut.*" ]

RUN chmod +x /entrypoint.sh

