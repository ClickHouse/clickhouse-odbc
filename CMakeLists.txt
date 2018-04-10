project (clickhouse-odbc)
cmake_minimum_required (VERSION 2.6)
cmake_policy(SET CMP0042 OLD)
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
set (CMAKE_DEBUG_POSTFIX                 "") # Dont add "d" to target lib name
list (REVERSE CMAKE_FIND_LIBRARY_SUFFIXES) # we want static link - now .a libs have highest priority

include (cmake/arch.cmake)

if (ARCH_FREEBSD)
    set (CMAKE_CXX_FLAGS                "${CMAKE_CXX_FLAGS} -U_XOPEN_SOURCE -UPOCO_HAVE_FD_EPOLL")
endif ()

if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "None")
    set (CMAKE_BUILD_TYPE "RELWITHDEBINFO")
endif ()
string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_UC)

include (cmake/sanitize.cmake)

# to avoid this error:
# /usr/bin/ld: /usr/lib/gcc/x86_64-linux-gnu/7/../../../x86_64-linux-gnu/libssl.a(s23_srvr.o): relocation R_X86_64_PC32 against symbol `ssl23_get_client_hello' can not be used when making a shared object; recompile with -fPIC
# use:  cmake .. -DFORCE_STATIC_LINK=
#set (FORCE_STATIC_LINK "-Bstatic," CACHE INTERNAL "")

if (NOT APPLE AND NOT ARCH_FREEBSD AND NOT WIN32)
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,${FORCE_STATIC_LINK}--whole-archive -static-libgcc -static-libstdc++ -Wl,--no-whole-archive")
endif ()
if (WIN32)
    #set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
endif ()



option (CLICKHOUSE_ODBC_TEST "Enable tests" 1)
if (CLICKHOUSE_ODBC_TEST)
    enable_testing ()
endif ()

option (ENABLE_SSL "Use openssl for accessing https:// interface" 0)

if (ENABLE_SSL)
    include (cmake/find_ssl.cmake)
    if (OPENSSL_FOUND)
        set (USE_SSL 1)
    endif ()
endif ()

option (UNICODE "Enable unicode (experimental)" 0)
if (UNICODE)
    set(UNICODE 1)
    add_definitions(-DUNICODE=1)
else ()
    set(UNICODE 0)
endif ()

set (OPENSSL_LIBRARIES "-Wl,-Bdynamic ${OPENSSL_LIBRARIES} -Wl,-Bstatic")

include (cmake/find_ltdl.cmake) # for odbc
include (cmake/find_odbc.cmake)
include (cmake/find_poco.cmake)
include (cmake/find_ccache.cmake)

include (contrib/poco/cmake/FindODBC.cmake)
if(WIN32)
   set(ODBC_LIBRARIES "odbc32" "odbccp32")
   message(STATUS "Windows native ODBC Support Enabled")
endif ()
message (STATUS "Using odbc: ${ODBC_INCLUDE_DIRECTORIES} : ${ODBC_LIBRARIES}")
if (NOT WIN32 AND NOT ODBC_LIBRARIES MATCHES "iodbc")
    include (cmake/Modules/CheckODBCversion.cmake)
endif ()

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

include (cmake/print_flags.cmake)
