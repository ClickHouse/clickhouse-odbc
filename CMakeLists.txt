project (clickhouse-odbc)
cmake_minimum_required (VERSION 2.6)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${clickhouse-odbc_SOURCE_DIR}/cmake/Modules")

include(GNUInstallDirs)

message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set (COMMON_WARNING_FLAGS "-Wall")	# -Werror is also added inside directories with our own code.
set (CXX_WARNING_FLAGS "-Wnon-virtual-dtor")

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_EXTENSIONS 0)
set (CMAKE_CXX_STANDARD_REQUIRED         ON)
set (CMAKE_POSITION_INDEPENDENT_CODE     ON)
set (CMAKE_BUILD_COLOR_MAKEFILE          ON)
set (CMAKE_CXX_FLAGS                     "${CMAKE_CXX_FLAGS} -O2 -g -fvisibility-inlines-hidden")

# to avoid this error:
# /usr/bin/ld: /usr/lib/gcc/x86_64-linux-gnu/7/../../../x86_64-linux-gnu/libssl.a(s23_srvr.o): relocation R_X86_64_PC32 against symbol `ssl23_get_client_hello' can not be used when making a shared object; recompile with -fPIC
# use:  cmake .. -DFORCE_STATIC_LINK=
set (FORCE_STATIC_LINK "-Bstatic," CACHE INTERNAL "")

set (CMAKE_SHARED_LINKER_FLAGS           "${CMAKE_SHARED_LINKER_FLAGS} -Wl,${FORCE_STATIC_LINK}--whole-archive -static-libgcc -static-libstdc++ -Wl,--no-whole-archive")

option (CLICKHOUSE_ODBC_TEST "Enable tests" 1)
if (CLICKHOUSE_ODBC_TEST)
    enable_testing ()
endif ()

option (ENABLE_SSL "Use openssl for accessing https:// interface" OFF)

if (ENABLE_SSL)
    find_package (OpenSSL)
    if (OPENSSL_FOUND)
        set (USE_SSL 1)
    endif ()
endif ()

set (OPENSSL_LIBRARIES "-Wl,-Bdynamic ${OPENSSL_LIBRARIES} -Wl,-Bstatic")

include (cmake/find_poco.cmake)
include (cmake/find_ccache.cmake)
include (contrib/poco/cmake/FindODBC.cmake)
include (cmake/Modules/CheckODBCversion.cmake)

message (STATUS "Building for: ${CMAKE_SYSTEM} ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_LIBRARY_ARCHITECTURE} ; CCACHE=${CCACHE_FOUND} ${CCACHE_VERSION}")

message (STATUS "${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_SYSTEM} ${CMAKE_SYSTEM_VERSION}")

add_subdirectory (contrib)

# after contrib/poco
if (USE_INTERNAL_POCO_LIBRARY)
    # Remove pthread from linking poco's Net
    set_property(TARGET Net PROPERTY INTERFACE_LINK_LIBRARIES "")
    set_property(TARGET Foundation PROPERTY INTERFACE_LINK_LIBRARIES "")
endif ()

add_subdirectory (driver)
