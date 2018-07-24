#!/usr/bin/env sh

# env CMAKE_FLAGS="-DCMAKE_CXX_COMPILER=`which clang++-6.0` -DCMAKE_C_COMPILER=`which clang-6.0`" sh -x ./test_all.sh

cd ..
for compiler in clang gcc; do
    if [ "$compiler" = "clang" ]; then
        CMAKE_COMPILER_FLAGS="-DCMAKE_CXX_COMPILER=`which clang++-6.0 clang++60 clang++ | head -n1` -DCMAKE_C_COMPILER=`which clang-6.0 clang60 clang | head -n1`"
    fi
    if [ "$compiler" = "gcc" ]; then
        CMAKE_COMPILER_FLAGS="-DCMAKE_CXX_COMPILER=`which g++-8 g++8 g++ | head -n1` -DCMAKE_C_COMPILER=`which gcc-8 gcc8 gcc | head -n1`"
    fi
    for type in asan tsan ubsan msan release relwithdebinfo debug; do
        mkdir -p build_${compiler}_$type
        rm -rf build
        ln -sf build_${compiler}_$type build
        cd build
        cmake .. -G Ninja -DCMAKE_BUILD_TYPE=$type -DTEST_DSN=${TEST_DSN=clickhouse_localhost} $CMAKE_COMPILER_FLAGS $CMAKE_FLAGS && cmake --build . -- -j ${MAKEJ=$(distcc -j || nproc || sysctl -n hw.ncpu || echo 4)} && ctest -V
        cd ..
    done
done
