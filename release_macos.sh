#!/usr/bin/env sh

brew install gcc ninja libiodbc
brew unlink unixodbc
brew link libiodbc
#brew install unixodbc;brew unlink libiodbc;brew link unixodbc

  #for compiler in _gcc; do
  for compiler in ""; do
    for type in relwithdebinfo; do
      for option in ""; do
        CMAKE_FLAGS_ADD=""
        if [ "$compiler" = "_gcc" ]; then
           CMAKE_FLAGS_ADD="${CMAKE_FLAGS_ADD} -DCMAKE_CXX_COMPILER=`which g++-8 g++-7 g++8 g++7 g++ | head -n1` -DCMAKE_C_COMPILER=`which gcc-7 gcc-8 gcc8 gcc7 gcc | head -n1`"
        fi
        build_dir=build${compiler}_$type$option
        echo build $compiler $type $option in ${build_dir}
        mkdir -p ${build_dir}
        cd ${build_dir}
        rm CMakeCache.txt
        cmake .. -G Ninja -DCMAKE_BUILD_TYPE=$type -DTEST_DSN=${TEST_DSN=clickhouse_localhost} -DTEST_DSN_W=${TEST_DSN=clickhouse_localhost_w} $CMAKE_FLAGS_ADD $CMAKE_FLAGS && cmake --build . -- -j ${MAKEJ=$(distcc -j || nproc || sysctl -n hw.ncpu || echo 4)}
        cd ..
        rm -rf build
        ln -sf ${build_dir} build
        cd ${build_dir}
        ctest -V
        cd ..
      done
    done
done
