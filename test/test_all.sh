#!/usr/bin/env sh

# env CMAKE_FLAGS="-DCMAKE_CXX_COMPILER=`which clang++-6.0` -DCMAKE_C_COMPILER=`which clang-6.0`" sh -x test_all.sh
# env CMAKE_FLAGS="-DUSE_INTERNAL_POCO_LIBRARY=0 -DUSE_INTERNAL_SSL_LIBRARY=0 -DCMAKE_CXX_COMPILER=`which clang++60` -DCMAKE_C_COMPILER=`which clang60`" sh -x test_all.sh

cd ..
  for compiler in "" _gcc _clang; do
    if [ "$compiler" = "_clang" ]; then
        CMAKE_COMPILER_FLAGS="-DCMAKE_CXX_COMPILER=`which clang++-6.0 clang++-5.0 clang++60 clang++50 clang++ | head -n1` -DCMAKE_C_COMPILER=`which clang-6.0 clang-5.0 clang60 clang50 clang | head -n1`"
    fi
    if [ "$compiler" = "_gcc" ]; then
        CMAKE_COMPILER_FLAGS="-DCMAKE_CXX_COMPILER=`which g++-8 g++-7 g++8 g++7 g++ | head -n1` -DCMAKE_C_COMPILER=`which gcc-8 gcc-7 gcc8 gcc7 gcc | head -n1`"
    fi
    for type in debug asan tsan ubsan msan release relwithdebinfo; do
      #for option in "-DUNICODE=1" ""; do
      for option in ""; do
        build_dir=build${compiler}_$type$option
        echo build $compiler $type $option in ${build_dir}
        mkdir -p ${build_dir}
        rm -rf build
        ln -sf ${build_dir} build
        cd build
        rm CMakeCache.txt
        cmake .. -G Ninja $option -DCMAKE_BUILD_TYPE=$type -DTEST_DSN=${TEST_DSN=clickhouse_localhost} -DTEST_DSN_W=${TEST_DSN=clickhouse_localhost_w} -DLIB_NAME_NO_W=1 $CMAKE_COMPILER_FLAGS $CMAKE_FLAGS && cmake --build . -- -j ${MAKEJ=$(distcc -j || nproc || sysctl -n hw.ncpu || echo 4)} && ctest -V
        cd ..
      done
    done
done
