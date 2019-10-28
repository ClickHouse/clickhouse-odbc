cmake_minimum_required (VERSION 3.2)

project (clickhouse-odbc VERSION 1.1.0.20191028 LANGUAGES C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
set(CMAKE_LINK_DEPENDS_NO_SHARED 1) # Do not relink all depended targets on .so
set(CMAKE_CXX_EXTENSIONS 0)
set(CMAKE_CXX_STANDARD_REQUIRED 1)
set(CMAKE_POSITION_INDEPENDENT_CODE 1)
set(CMAKE_BUILD_COLOR_MAKEFILE 1)
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME ANSIDriver)

if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "None")
    set (CMAKE_BUILD_TYPE "RelWithDebInfo")
endif ()

if (NOT CMAKE_CONFIGURATION_TYPES)
    set (CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}")
endif()

message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

string (TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UC)
string (TOUPPER "${CMAKE_CONFIGURATION_TYPES}" CMAKE_CONFIGURATION_TYPES_UC)

set (COMMON_WARNING_FLAGS "-Wall")	# -Werror is also added inside directories with our own code.
set (CXX_WARNING_FLAGS "-Wnon-virtual-dtor")

set (CMAKE_CXX_STANDARD 14)
# For iostream_debug_helpers.h :
if (CMAKE_BUILD_TYPE_UC STREQUAL "DEBUG" AND USE_DEBUG_17)
    set (CMAKE_CXX_STANDARD 17)
endif ()

include(GNUInstallDirs)

if (MSVC)
    foreach (config ${CMAKE_CONFIGURATION_TYPES_UC})
        foreach (lang CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
            set (var "${lang}_${config}")
            string (REPLACE "/MD" "/MT" ${var} "${${var}}")
            string (REPLACE "-MD" "-MT" ${var} "${${var}}")
        endforeach ()
    endforeach ()
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
else ()
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
endif ()

if (NOT BUILD_SHARED)
    list (REVERSE CMAKE_FIND_LIBRARY_SUFFIXES) # we want static link - now .a libs have highest priority
    if (COMPILER_GCC)
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif ()
endif ()

message (STATUS "BUILD_SHARED: ${BUILD_SHARED}")

include (cmake/arch.cmake)

if (ARCH_FREEBSD)
    set (CMAKE_CXX_FLAGS                "${CMAKE_CXX_FLAGS} -U_XOPEN_SOURCE -UPOCO_HAVE_FD_EPOLL")
endif ()

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

add_subdirectory (contrib EXCLUDE_FROM_ALL)

foreach (config ${CMAKE_CONFIGURATION_TYPES_UC})
    set(CMAKE_${config}_POSTFIX "" CACHE STRING "" FORCE) # Don't append "d" or "md" to output lib name
endforeach ()

add_subdirectory (driver)

if(CLICKHOUSE_ODBC_TEST)
    add_subdirectory(test)
endif()

include (cmake/print_flags.cmake)

if(NOT WIN32)
    install(
        FILES packaging/unixodbc.ini packaging/odbc.ini
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/sample-config
        COMPONENT Documentation
    )
endif()

set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT RuntimeLibraries)
include(InstallRequiredSystemLibraries)

include(CPackComponent)

# Remove empty/irrelevant components, mostly brought by third-party projects.
get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
list(REMOVE_ITEM CPACK_COMPONENTS_ALL "Devel")

cpack_add_component_group(ANSIGroup
    DISPLAY_NAME "ANSI ${ARCH_BITS}-bit Driver"
    DESCRIPTION "ClickHouse ODBC Driver (ANSI, ${ARCH_BITS}-bit)"
    EXPANDED
)
cpack_add_component(ANSIDriver
    DISPLAY_NAME "Driver"
    DESCRIPTION "ClickHouse ODBC Driver (ANSI, ${ARCH_BITS}-bit)"
    REQUIRED
    GROUP ANSIGroup
)
cpack_add_component(ANSIDriverDebugSymbols
    DISPLAY_NAME "Debug symbols"
    DESCRIPTION "Debug symbols (PDB) for the Driver (ANSI, ${ARCH_BITS}-bit)"
    DISABLED
    DEPENDS ANSIDriver
    GROUP ANSIGroup
)
cpack_add_component_group(UnicodeGroup
    DISPLAY_NAME "Unicode ${ARCH_BITS}-bit Driver"
    DESCRIPTION "ClickHouse ODBC Driver (Unicode, ${ARCH_BITS}-bit)"
    EXPANDED
)
cpack_add_component(UnicodeDriver
    DISPLAY_NAME "Driver"
    DESCRIPTION "ClickHouse ODBC Driver (Unicode, ${ARCH_BITS}-bit)"
    DEPENDS ANSIDriver
    GROUP UnicodeGroup
)
cpack_add_component(UnicodeDriverDebugSymbols
    DISPLAY_NAME "Debug symbols"
    DESCRIPTION "Debug symbols (PDB) for Driver (Unicode, ${ARCH_BITS}-bit)"
    DISABLED
    DEPENDS UnicodeDriver
    GROUP UnicodeGroup
)
cpack_add_component(RuntimeLibraries
    DISPLAY_NAME "Runtime Libraries"
    DESCRIPTION "System/language redistributable runtime libraries"
    DEPENDS ANSIDriver
)

if(WIN32 AND NOT UNIX)
    set(CPACK_GENERATOR "ZIP;WIX")
elseif(APPLE)
    set(CPACK_GENERATOR "TXZ")
elseif(UNIX_RHEL OR UNIX_FEDORA)
    set(CPACK_GENERATOR "TXZ;RPM")
elseif(UNIX_DEBIAN OR UNIX_UBUNTU)
    set(CPACK_GENERATOR "TXZ;DEB")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

if(WIN32 AND NOT UNIX)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
else()
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "/usr/local")
endif()

set(CPACK_PACKAGE_VENDOR "Yandex LLC")
set(CPACK_PACKAGE_DESCRIPTION "ClickHouse ODBC Driver (${ARCH_BITS}-bit)")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ClickHouse ODBC Driver (${ARCH_BITS}-bit)")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/packaging/Readme.rtf")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/packaging/License.rtf")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ClickHouse/clickhouse-odbc")

set(CPACK_WIX_ROOT_FEATURE_TITLE "ClickHouse ODBC Driver")
set(CPACK_WIX_ROOT_FEATURE_DESCRIPTION "ClickHouse ODBC Driver (${ARCH_BITS}-bit)")
set(CPACK_WIX_PATCH_FILE "${PROJECT_SOURCE_DIR}/packaging/RegConfig.patch.wxs")
if ("${ARCH_BITS}" STREQUAL "32")
    set(CPACK_WIX_PRODUCT_GUID "9FCA44DB-6963-4EBF-87A6-279331C139EB")
    set(CPACK_WIX_UPGRADE_GUID "3C19591C-7FFC-461A-8828-611EDFBE0619")
elseif("${ARCH_BITS}" STREQUAL "64")
    set(CPACK_WIX_PRODUCT_GUID "D77FCBEA-C3A9-442C-8055-83CBDB57009B")
    set(CPACK_WIX_UPGRADE_GUID "B63A7326-E080-49E3-A963-8F5EFC2DDD65")
endif()

include(CPack)
