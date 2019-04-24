project (clickhouse-odbc)
cmake_minimum_required (VERSION 2.6)
cmake_policy(SET CMP0023 NEW)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
set(CMAKE_LINK_DEPENDS_NO_SHARED 1) # Do not relink all depended targets on .so
set(CMAKE_CXX_EXTENSIONS 0)
set(CMAKE_CXX_STANDARD_REQUIRED 1)
set(CMAKE_POSITION_INDEPENDENT_CODE 1)
set(CMAKE_BUILD_COLOR_MAKEFILE 1)
set(CMAKE_DEBUG_POSTFIX "") # Dont add "d" to target lib name


message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

set (COMMON_WARNING_FLAGS "-Wall")	# -Werror is also added inside directories with our own code.
set (CXX_WARNING_FLAGS "-Wnon-virtual-dtor")

set (CMAKE_CXX_STANDARD 14)
# For iostream_debug_helpers.h :
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND USE_DEBUG_17)
    set (CMAKE_CXX_STANDARD 17)
endif ()

include(GNUInstallDirs)

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

if (NOT BUILD_SHARED)
    list (REVERSE CMAKE_FIND_LIBRARY_SUFFIXES) # we want static link - now .a libs have highest priority
    if (COMPILER_GCC)
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif ()
endif ()

message (STATUS "BUILD_SHARED=${BUILD_SHARED}")

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

option (UNBUNDLED "Try find all libraries in system (if fail - use bundled from contrib/)" OFF)
if (UNBUNDLED)
    set(NOT_UNBUNDLED 0)
else ()
    set(NOT_UNBUNDLED 1)
endif ()

option (ENABLE_SSL "Use openssl for accessing https:// interface" 1)

if (ENABLE_SSL)
    include (cmake/find_ssl.cmake)
    if (OPENSSL_FOUND)
        set (USE_SSL 1)
    endif ()
endif ()

include (cmake/find_ltdl.cmake) # for odbc
include (cmake/find_odbc.cmake)

#if (NOT WIN32)
#    set (OPENSSL_LIBRARIES "-Wl,-Bdynamic ${OPENSSL_LIBRARIES} -Wl,-Bstatic")
#endif ()

include (cmake/find_poco.cmake)
include (cmake/find_nanoodbc.cmake)
include (cmake/find_ccache.cmake)

if (EXISTS contrib/poco/cmake/FindODBC.cmake)
    include (contrib/poco/cmake/FindODBC.cmake)
else ()
    include (cmake/find_odbc.cmake)
endif ()

if(WIN32)
   set(ODBC_LIBRARIES "odbc32" "odbccp32")
   message(STATUS "Windows native ODBC Support Enabled")
endif ()
message (STATUS "Using odbc: ${ODBC_INCLUDE_DIRECTORIES} : ${ODBC_LIBRARIES}")
#if(NOT WIN32 AND NOT ODBC_LIBRARIES MATCHES "iodbc")
#    include(cmake/Modules/CheckODBCversion.cmake)
#endif()

message (STATUS "Building for: ${CMAKE_SYSTEM} ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_LIBRARY_ARCHITECTURE} ; CCACHE=${CCACHE_FOUND} ${CCACHE_VERSION}")

add_subdirectory (contrib)
add_subdirectory (driver)

if(CLICKHOUSE_ODBC_TEST)
    add_subdirectory(test)
endif()

include (cmake/print_flags.cmake)
