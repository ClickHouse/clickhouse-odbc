#!/usr/bin/env bash

# !!! WARNING! THIS SCRIPT WILL UNINSTALL ALL PACKAGES DEPEND ON libiodbc2-dev unixodbc-dev !!!

# To build with your test unixodbc:
# env CMAKE_FLAGS="-DODBC_LIBRARIES=$HOME/unixODBC-2.3.7/odbcinst/.libs/libodbcinst.so;$HOME/unixODBC-2.3.7/DriverManager/.libs/libodbc.so" USE_LIBS="unixodbc" sh -x ./test_unicode.sh

set -x

cd ..
 for lib in ${USE_LIBS=libiodbc unixodbc}; do

    if [ "$lib" = "libiodbc" ]; then
        CMAKE_FLAGS0=" -DFIND_IODBC_FIRST=1 "
    fi
    if [ "$lib" = "unixodbc" ]; then
        CMAKE_FLAGS0=" -DFIND_UNIXODBC_FIRST=1 "
    fi
if [[ "$OSTYPE" == "darwin"* ]]; then
    if [ "$lib" = "libiodbc" ]; then
         brew unlink unixodbc
         brew install $lib
         brew link $lib
    fi
    if [ "$lib" = "unixodbc" ]; then
         brew unlink libiodbc
         brew install $lib
         brew link $lib
    fi
elif [[ "$OSTYPE" == "FreeBSD"* ]]; then
    if [ "$lib" = "libiodbc" ]; then
        pkg install -y $lib
    fi
    if [ "$lib" = "unixodbc" ]; then
        pkg install -y $lib
    fi
elif [ `which apt` ]; then
    if [ "$lib" = "libiodbc" ]; then
        sudo apt install -y libiodbc2-dev iodbc
    fi
    if [ "$lib" = "unixodbc" ]; then
        sudo apt install -y unixodbc-dev unixodbc
    fi
fi
  for compiler in ""; do
    if [ "$compiler" = "_clang" ]; then
        CMAKE_COMPILER_FLAGS="-DCMAKE_CXX_COMPILER=`which clang++-8 clang++-7 clang++-6.0 clang++-5.0 clang++60 clang++50 clang++ | head -n1` -DCMAKE_C_COMPILER=`which clang-8 clang-7 clang-6.0 clang-5.0 clang60 clang50 clang | head -n1`"
    fi
    if [ "$compiler" = "_gcc" ]; then
        CMAKE_COMPILER_FLAGS="-DCMAKE_CXX_COMPILER=`which g++-9 g++-8 g++-7 g++9 g++8 g++7 g++ | head -n1` -DCMAKE_C_COMPILER=`which gcc-9 gcc-8 gcc-7 gcc9 gcc8 gcc7 gcc | head -n1`"
    fi
    for type in ${USE_TYPE=debug}; do
      for option in ""; do
        build_dir=build${compiler}_$type$option
        echo build $compiler $type $option in ${build_dir}
        mkdir -p ${build_dir}
        rm -rf build
        ln -sf ${build_dir} build
        cd build
        rm CMakeCache.txt
        cmake .. -G Ninja $option -DCMAKE_BUILD_TYPE=$type -DTEST_DSN=${TEST_DSN=clickhouse_localhost} -DTEST_DSN_W=${TEST_DSN_W=clickhouse_localhost_w} $CMAKE_COMPILER_FLAGS $CMAKE_FLAGS0 $CMAKE_FLAGS | tee log_cmake.log && ninja -j ${MAKEJ=$(distcc -j || nproc || sysctl -n hw.ncpu || echo 4)} | tee log_build.log && ctest $CTEST_OPT | tee log_test.log
        cd ..
      done
    done
 done
done
