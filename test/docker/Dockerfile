FROM yandex/clickhouse-server

# image to build & test clickhouse odbc on ubuntu & docker
# install all needed build & test dependancies

# docker build . -t clickhouse-odbc-tester

RUN apt-get update -y \
    && env DEBIAN_FRONTEND=noninteractive \
    && BUILD_PACKAGES="build-essential ninja-build libiodbc2-dev cmake git libltdl-dev perl libdbi-perl libdbd-odbc-perl python python-pyodbc" \
    && RUNTIME_PACKAGES="unixodbc" \
    && apt-get install -y $BUILD_PACKAGES $RUNTIME_PACKAGES

RUN git clone --recursive https://github.com/ClickHouse/clickhouse-odbc.git

RUN mkdir -p clickhouse-odbc/build \
    && cd clickhouse-odbc/build \
    && cmake -G Ninja -DCH_ODBC_TEST_DSN_LIST="clickhouse_localhost;clickhouse_localhost_w" .. \
    && ninja

RUN ln -s /clickhouse-odbc/build/driver/libclickhouseodbc.so /usr/local/lib/libclickhouseodbc.so \
    && ln -s /clickhouse-odbc/build/driver/libclickhouseodbcw.so /usr/local/lib/libclickhouseodbcw.so \
    && echo '' >> ~/.odbc.ini \
    && echo '[clickhouse_localhost]' >> ~/.odbc.ini \
    && echo 'Driver=/usr/local/lib/libclickhouseodbc.so' >> ~/.odbc.ini \
    && echo 'url=http://localhost' >> ~/.odbc.ini \
    && echo '' >> ~/.odbc.ini \
    && echo '[clickhouse_localhost_w]' >> ~/.odbc.ini \
    && echo 'Driver=/usr/local/lib/libclickhouseodbcw.so' >> ~/.odbc.ini \
    && echo 'url=http://localhost' >> ~/.odbc.ini

# put the test into docker-entrypoint-initdb.d to run the tests on container initialization
# it just was the simplest
RUN echo '#!/bin/bash' > /docker-entrypoint-initdb.d/run_ctest.sh \
    && echo 'cd clickhouse-odbc/build' >> /docker-entrypoint-initdb.d/run_ctest.sh \
    && echo 'ctest -V' >> /docker-entrypoint-initdb.d/run_ctest.sh

# run the tests build time too:
RUN bash -c '/entrypoint.sh bash -c "echo ok"'
