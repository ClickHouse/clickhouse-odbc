project (clickhouse-odbc)
cmake_minimum_required (VERSION 2.6)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${clickhouse-odbc_SOURCE_DIR}/cmake/Modules")

include(GNUInstallDirs)

message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set (COMMON_WARNING_FLAGS "-Wall")	# -Werror is also added inside directories with our own code.
set (CXX_WARNING_FLAGS "-Wnon-virtual-dtor")

set (CMAKE_CXX_STANDARD 14)
# For iostream_debug_helpers.h :
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND USE_DEBUG_17)
    set (CMAKE_CXX_STANDARD 17)
endif ()

set (CMAKE_CXX_EXTENSIONS 0)
set (CMAKE_CXX_STANDARD_REQUIRED         ON)
set (CMAKE_POSITION_INDEPENDENT_CODE     ON)
set (CMAKE_BUILD_COLOR_MAKEFILE          ON)
if (NOT MSVC)
	set (CMAKE_CXX_FLAGS                     "${CMAKE_CXX_FLAGS} -O2 -g -fvisibility-inlines-hidden")
endif ()
if (MSVC)
	set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
	set (CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
	set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	set (CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set (CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
	set (CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
	set (CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif ()
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

option (ENABLE_SSL "Use openssl for accessing https:// interface" 1)

if (ENABLE_SSL)
    set (USE_INTERNAL_SSL_LIBRARY 1 CACHE INTERNAL "")
    include (cmake/find_ssl.cmake)
    if (OPENSSL_FOUND)
        set (USE_SSL 1)
    endif ()
endif ()

option (UNICODE "Enable unicode (experimental)" 0)
if (UNICODE)
    set(UNICODE 1)
    add_definitions(-DUNICODE=1 -D_UNICODE=1)
else ()
    set(UNICODE 0)
endif ()

if (NOT WIN32)
    set (OPENSSL_LIBRARIES "-Wl,-Bdynamic ${OPENSSL_LIBRARIES} -Wl,-Bstatic")
endif ()

include (cmake/find_ltdl.cmake) # for odbc
include (cmake/find_odbc.cmake)
include (cmake/find_poco.cmake)
include (cmake/find_nanoodbc.cmake)
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
add_subdirectory (driver)

if(CLICKHOUSE_ODBC_TEST)
    message(STATUS "TEST_DSN=${TEST_DSN}")
    add_subdirectory(test)
endif()

include (cmake/print_flags.cmake)
