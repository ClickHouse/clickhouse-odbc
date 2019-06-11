#!/usr/bin/env sh

# env CMAKE_FLAGS="-DCMAKE_CXX_COMPILER=`which clang++-6.0` -DCMAKE_C_COMPILER=`which clang-6.0`" sh -x test_all.sh
# env CMAKE_FLAGS="-DUSE_INTERNAL_POCO_LIBRARY=0 -DUSE_INTERNAL_SSL_LIBRARY=0 -DCMAKE_CXX_COMPILER=`which clang++60` -DCMAKE_C_COMPILER=`which clang60`" sh -x test_all.sh

set -x

cd ..
for compiler in ${USE_COMPILER=_gcc _clang}; do
    if [ "$compiler" = "_clang" ]; then
        CMAKE_COMPILER_FLAGS="-DCMAKE_CXX_COMPILER=`which clang++-8 clang++-7 clang++-6.0 clang++-5.0 clang++60 clang++50 clang++ | head -n1` -DCMAKE_C_COMPILER=`which clang-8 clang-7 clang-6.0 clang-5.0 clang60 clang50 clang | head -n1`"
    fi
    if [ "$compiler" = "_gcc" ]; then
        CMAKE_COMPILER_FLAGS="-DCMAKE_CXX_COMPILER=`which g++-9 g++-8 g++-7 g++9 g++8 g++7 g++ | head -n1` -DCMAKE_C_COMPILER=`which gcc-9 gcc-8 gcc-7 gcc9 gcc8 gcc7 gcc | head -n1`"
    fi
    for type in ${USE_TYPE=debug asan tsan ubsan release relwithdebinfo}; do
      for option in ""; do
        CTEST_ENV0=""
        if [ "$type" = "asan" ]; then
          CTEST_ENV0="LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libasan.so.5"
        fi
        if [ "$type" = "tsan" ]; then
          CTEST_ENV0="LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtsan.so.0"
        fi
        if [ "$type" = "ubsan" ]; then
          CTEST_ENV0="LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libubsan.so.1"
        fi
        build_dir=build${compiler}_$type$option
        echo build $compiler $type $option in ${build_dir}
        mkdir -p ${build_dir}
        rm -rf build
        ln -sf ${build_dir} build
        cd build
        rm CMakeCache.txt
        cmake .. -G Ninja $option -DCMAKE_BUILD_TYPE=$type -DTEST_DSN=${TEST_DSN=clickhouse_localhost} -DTEST_DSN_W=${TEST_DSN_W=clickhouse_localhost_w} $CMAKE_COMPILER_FLAGS $CMAKE_FLAGS | tee log_cmake.log && ninja -j ${MAKEJ=$(distcc -j || nproc || sysctl -n hw.ncpu || echo 4)} | tee log_build.log && env $CTEST_ENV0 $CTEST_ENV ctest $CTEST_OPT | tee log_ctest.log
        cd ..
      done
    done
done
