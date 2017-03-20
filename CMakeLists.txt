project (clickhouse-odbc)
cmake_minimum_required (VERSION 2.6)

message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

set (COMMON_WARNING_FLAGS "-Wall")	# -Werror is also added inside directories with our own code.
set (CXX_WARNING_FLAGS "-Wnon-virtual-dtor")

set (CMAKE_BUILD_COLOR_MAKEFILE          ON)
set (CMAKE_CXX_FLAGS                     "-std=c++14 -O2 -g -fPIC -fvisibility-inlines-hidden -Wl,-Bstatic,--whole-archive -static-libgcc -static-libstdc++ -Wl,--no-whole-archive")

find_library(Poco_LIBRARIES NAMES PocoNet PocoFoundation)

message (STATUS "Building for: ${CMAKE_SYSTEM} ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_LIBRARY_ARCHITECTURE}")

add_subdirectory (driver)
