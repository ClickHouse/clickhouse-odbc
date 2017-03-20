project (clickhouse-odbc)
cmake_minimum_required (VERSION 2.6)

message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

set (COMMON_WARNING_FLAGS "-Wall")	# -Werror is also added inside directories with our own code.
set (CXX_WARNING_FLAGS "-Wnon-virtual-dtor")

set (CMAKE_BUILD_COLOR_MAKEFILE          ON)
set (CMAKE_CXX_FLAGS                     "${CMAKE_CXX_FLAGS} ${COMPILER_FLAGS} -std=c++14 ${PLATFORM_EXTRA_CXX_FLAG} -fno-omit-frame-pointer ${COMMON_WARNING_FLAGS} ${CXX_WARNING_FLAGS} ${GLIBC_COMPATIBILITY_COMPILE_FLAGS}")


set(POCO_PREFIX "/usr/local")
set(POCO_INCLUDE_DIR"${POCO_PREFIX}/include")
set(POCO_LIB_DIR "${POCO_PREFIX}/lib")
set(POCO_LIBS
        "${POCO_LIB_DIR}/libPocoNet.so"
        "${POCO_LIB_DIR}/libPocoUtil.so"
        "${POCO_LIB_DIR}/libPocoFoundation.so")

message (STATUS "Building for: ${CMAKE_SYSTEM} ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_LIBRARY_ARCHITECTURE}")

add_subdirectory (driver)
