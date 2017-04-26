project (clickhouse-odbc)
cmake_minimum_required (VERSION 2.6)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${clickhouse-odbc_SOURCE_DIR}/cmake/Modules")

message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set (COMMON_WARNING_FLAGS "-Wall")	# -Werror is also added inside directories with our own code.
set (CXX_WARNING_FLAGS "-Wnon-virtual-dtor")

set (CMAKE_BUILD_COLOR_MAKEFILE          ON)
set (CMAKE_CXX_FLAGS                     "${CMAKE_CXX_FLAGS} -std=c++14 -O2 -g -fvisibility-inlines-hidden")
set (CMAKE_SHARED_LINKER_FLAGS           "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -static-libgcc -static-libstdc++ -Wl,--no-whole-archive")
set (CMAKE_POSITION_INDEPENDENT_CODE     ON)

# === find_poco ===
option (USE_INTERNAL_POCO_LIBRARY "Set to FALSE to use system poco library instead of bundled" 1)

if (NOT USE_INTERNAL_POCO_LIBRARY)

    if (WIN32 OR MSVC)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib")
    elseif (UNIX)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    endif ()

    find_package (Poco COMPONENTS Net)
endif ()

if (Poco_INCLUDE_DIRS AND Poco_Foundation_LIBRARY)
    include_directories (${Poco_INCLUDE_DIRS})
else ()
    set (POCO_STATIC 1 CACHE BOOL "")

    set (ENABLE_CPPUNIT 0 CACHE BOOL "")
    set (ENABLE_XML 0 CACHE BOOL "")
    set (ENABLE_JSON 0 CACHE BOOL "")
    set (ENABLE_MONGODB 0 CACHE BOOL "")
    set (ENABLE_NETSSL 0 CACHE BOOL "")
    set (ENABLE_CRYPTO 0 CACHE BOOL "")
    set (ENABLE_DATA 0 CACHE BOOL "")
    set (ENABLE_ZIP 0 CACHE BOOL "")
    set (ENABLE_PAGECOMPILER 0 CACHE BOOL "")
    set (ENABLE_PAGECOMPILER_FILE2PAGE 0 CACHE BOOL "")
    set (ENABLE_REDIS 0 CACHE BOOL "")

    set (USE_INTERNAL_POCO_LIBRARY 1)

    set (Poco_INCLUDE_DIRS
        "${clickhouse-odbc_SOURCE_DIR}/contrib/poco/Foundation/include/"
        "${clickhouse-odbc_SOURCE_DIR}/contrib/poco/Util/include/"
        "${clickhouse-odbc_SOURCE_DIR}/contrib/poco/Net/include/"
    )

    set (Poco_Foundation_LIBRARY PocoFoundation)
    set (Poco_Util_LIBRARY PocoUtil)
    set (Poco_Net_LIBRARY PocoNet)
    include_directories (BEFORE ${Poco_INCLUDE_DIRS})
endif ()

message(STATUS "Using Poco: ${Poco_INCLUDE_DIRS} : ${Poco_Foundation_LIBRARY},${Poco_Util_LIBRARY},${Poco_Net_LIBRARY}")
# === find_poco_end ===

include (contrib/poco/cmake/FindODBC.cmake)

message (STATUS "Building for: ${CMAKE_SYSTEM} ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_LIBRARY_ARCHITECTURE}")

add_subdirectory (contrib)

# after contrib/poco
if (USE_INTERNAL_POCO_LIBRARY)
    # Remove pthread from linking poco's Net
    set_property(TARGET Net PROPERTY INTERFACE_LINK_LIBRARIES "")
    set_property(TARGET Foundation PROPERTY INTERFACE_LINK_LIBRARIES "")
endif ()

add_subdirectory (driver)
