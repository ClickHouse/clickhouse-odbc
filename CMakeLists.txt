cmake_minimum_required (VERSION 3.9)

set (CMAKE_MODULE_PATH_orig ${CMAKE_MODULE_PATH})
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
include (cmake/prevent_in_source_builds.cmake)

project (clickhouse-odbc VERSION 1.1.2.20191216 LANGUAGES C CXX)

set (CMAKE_C_STANDARD 11)
#set (CMAKE_C_EXTENSIONS 0)
set (CMAKE_C_STANDARD_REQUIRED 1)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_EXTENSIONS 0)
set (CMAKE_CXX_STANDARD_REQUIRED 1)

set (CMAKE_POSITION_INDEPENDENT_CODE 1)

set_property (GLOBAL PROPERTY USE_FOLDERS 1)
set (CMAKE_BUILD_COLOR_MAKEFILE 1)
set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME ANSIDriver)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "RelWithDebInfo")
endif ()

if (NOT CMAKE_CONFIGURATION_TYPES)
    set (CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}")
endif ()

message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

string (TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UC)
string (TOUPPER "${CMAKE_CONFIGURATION_TYPES}" CMAKE_CONFIGURATION_TYPES_UC)

include(CMakeDependentOption)

option(BUILD_TESTING "Build test targets" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

option (CH_ODBC_ENABLE_SSL "Enable SSL (required for utilizing https:// interface, etc.)" ON)
option (CH_ODBC_ENABLE_INSTALL "Enable install targets (required for packaging)" ON)
cmake_dependent_option (CH_ODBC_ENABLE_TESTING "Enable test targets" ON "BUILD_TESTING" OFF)
option (CH_ODBC_PREFER_BUNDLED_THIRD_PARTIES "Prefer bundled over system variants of third party libraries" ON)
cmake_dependent_option (CH_ODBC_PREFER_BUNDLED_POCO "Prefer bundled over system variants of Poco library" ON "CH_ODBC_PREFER_BUNDLED_THIRD_PARTIES" OFF)
cmake_dependent_option (CH_ODBC_PREFER_BUNDLED_SSL "Prefer bundled over system variants of SSL library" ON "CH_ODBC_PREFER_BUNDLED_POCO" OFF)
cmake_dependent_option (CH_ODBC_PREFER_BUNDLED_GOOGLETEST "Prefer bundled over system variants of Google Test library" ON "CH_ODBC_PREFER_BUNDLED_THIRD_PARTIES" OFF)
cmake_dependent_option (CH_ODBC_PREFER_BUNDLED_NANODBC "Prefer bundled over system variants of nanodbc library" ON "CH_ODBC_PREFER_BUNDLED_THIRD_PARTIES" OFF)
option (CH_ODBC_RUNTIME_LINK_STATIC "Link with compiler and language runtime statically" OFF)
option (CH_ODBC_THIRD_PARTY_LINK_STATIC "Link with third party libraries statically" ON)

set (CH_ODBC_DEFAULT_DSN_ANSI "ClickHouse DSN (ANSI)")
set (CH_ODBC_DEFAULT_DSN_UNICODE "ClickHouse DSN (Unicode)")

if (MSVC)
    # This default encoding mode will be overriden by UNICODE, in the corresponding cases.
    add_compile_definitions (SBCS _SBCS)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
endif ()

if (ARCH_FREEBSD)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U_XOPEN_SOURCE -UPOCO_HAVE_FD_EPOLL")
endif ()

if (CH_ODBC_RUNTIME_LINK_STATIC)
    if (MSVC)
        foreach (config ${CMAKE_CONFIGURATION_TYPES_UC})
            foreach (lang CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
                set (var "${lang}_${config}")
                string (REPLACE "/MD" "/MT" ${var} "${${var}}")
                string (REPLACE "-MD" "-MT" ${var} "${${var}}")
            endforeach ()
        endforeach ()
    endif ()

    if (COMPILER_GCC)
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif ()

    if (NOT APPLE AND NOT ARCH_FREEBSD AND NOT WIN32)
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,${FORCE_STATIC_LINK}--whole-archive -static-libgcc -static-libstdc++ -Wl,--no-whole-archive")
    endif ()
endif ()

include (GNUInstallDirs)
include (cmake/arch.cmake)
include (cmake/sanitize.cmake)

find_package (Threads REQUIRED)
find_package (ODBC REQUIRED)

if (CH_ODBC_ENABLE_SSL)
    if (NOT CH_ODBC_PREFER_BUNDLED_SSL)
        if (CH_ODBC_THIRD_PARTY_LINK_STATIC)
            set (OPENSSL_USE_STATIC_LIBS TRUE)
        endif ()

        if (CH_ODBC_RUNTIME_LINK_STATIC)
            set (OPENSSL_MSVC_STATIC_RT TRUE)
        endif ()

        find_package (OpenSSL)
    endif ()
endif ()

if (NOT CH_ODBC_PREFER_BUNDLED_POCO)
    find_package (Poco COMPONENTS Foundation Net NetSSL)
endif ()

if (CH_ODBC_ENABLE_TESTING)
    if (NOT CH_ODBC_PREFER_BUNDLED_GOOGLETEST)
        find_package (GTest)
    endif ()

    if (CH_ODBC_RUNTIME_LINK_STATIC)
        set (GTEST_MSVC_SEARCH "MT")
    else ()
        set (GTEST_MSVC_SEARCH "MD")
    endif ()

    if (NOT CH_ODBC_PREFER_BUNDLED_NANODBC)
        message (WARNING "nanodbc: using system variant of the library currently not supported")
#       find_package (nanodbc)
    endif ()
endif ()

add_subdirectory (contrib EXCLUDE_FROM_ALL)

if (CH_ODBC_ENABLE_TESTING)
    include (CTest)
    enable_testing ()
    include (GoogleTest)

    if (NOT TEST_DSN)
        set (TEST_DSN "${CH_ODBC_DEFAULT_DSN_ANSI}")
    endif ()

    if (NOT TEST_DSN_W)
        set (TEST_DSN_W "${CH_ODBC_DEFAULT_DSN_UNICODE}")
    endif ()
endif ()

foreach (config ${CMAKE_CONFIGURATION_TYPES_UC})
    set (CMAKE_${config}_POSTFIX "" CACHE STRING "" FORCE) # Don't append "d" or "md" to output lib name
endforeach ()

set (CMAKE_C_VISIBILITY_PRESET hidden)
set (CMAKE_CXX_VISIBILITY_PRESET hidden)
set (CMAKE_VISIBILITY_INLINES_HIDDEN 1)

include (CheckIPOSupported)
check_ipo_supported (RESULT ipo_supported LANGUAGES C CXX)
if (ipo_supported)
    set (CMAKE_INTERPROCEDURAL_OPTIMIZATION 1)
    set (CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG 0)
endif ()

add_subdirectory (driver)
if (CH_ODBC_ENABLE_TESTING)
    add_subdirectory (test)
endif ()

if (CH_ODBC_ENABLE_INSTALL)
    if (NOT WIN32)
        install (
            FILES packaging/odbcinst.ini.sample packaging/odbc.ini.sample
            DESTINATION ${CMAKE_INSTALL_DOCDIR}/config
            COMPONENT Documentation
        )
    endif ()

    set (CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT RuntimeLibraries)
    include (InstallRequiredSystemLibraries)

    include (CPackComponent)

    # Remove empty/irrelevant components, mostly brought by third-party projects.
    get_cmake_property (CPACK_COMPONENTS_ALL COMPONENTS)
    list (REMOVE_ITEM CPACK_COMPONENTS_ALL "Devel")

    cpack_add_component_group (ANSIGroup
        DISPLAY_NAME "ANSI ${ARCH_BITS}-bit Driver"
        DESCRIPTION "ClickHouse ODBC Driver (ANSI, ${ARCH_BITS}-bit)"
        EXPANDED
    )
    cpack_add_component (ANSIDriver
        DISPLAY_NAME "Driver"
        DESCRIPTION "ClickHouse ODBC Driver (ANSI, ${ARCH_BITS}-bit)"
        REQUIRED
        GROUP ANSIGroup
    )
    cpack_add_component (ANSIDriverDebugSymbols
        DISPLAY_NAME "Debug symbols"
        DESCRIPTION "Debug symbols (PDB) for the Driver (ANSI, ${ARCH_BITS}-bit)"
        DISABLED
        DEPENDS ANSIDriver
        GROUP ANSIGroup
    )
    cpack_add_component_group (UnicodeGroup
        DISPLAY_NAME "Unicode ${ARCH_BITS}-bit Driver"
        DESCRIPTION "ClickHouse ODBC Driver (Unicode, ${ARCH_BITS}-bit)"
        EXPANDED
    )
    cpack_add_component (UnicodeDriver
        DISPLAY_NAME "Driver"
        DESCRIPTION "ClickHouse ODBC Driver (Unicode, ${ARCH_BITS}-bit)"
        DEPENDS ANSIDriver
        GROUP UnicodeGroup
    )
    cpack_add_component (UnicodeDriverDebugSymbols
        DISPLAY_NAME "Debug symbols"
        DESCRIPTION "Debug symbols (PDB) for Driver (Unicode, ${ARCH_BITS}-bit)"
        DISABLED
        DEPENDS UnicodeDriver
        GROUP UnicodeGroup
    )
    cpack_add_component (RuntimeLibraries
        DISPLAY_NAME "Runtime Libraries"
        DESCRIPTION "System/language redistributable runtime libraries"
        DEPENDS ANSIDriver
    )

    if (WIN32 AND NOT UNIX)
        set (CPACK_GENERATOR "ZIP;WIX")
    elseif (APPLE)
        set (CPACK_GENERATOR "TXZ")
    elseif (UNIX_RHEL OR UNIX_FEDORA)
        set (CPACK_GENERATOR "TXZ;RPM")
    elseif (UNIX_DEBIAN OR UNIX_UBUNTU)
        set (CPACK_GENERATOR "TXZ;DEB")
    else ()
        set (CPACK_GENERATOR "TGZ")
    endif ()

    if (WIN32 AND NOT UNIX)
        set (CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    else ()
        set (CPACK_PACKAGE_INSTALL_DIRECTORY "/usr/local")
    endif ()

    set (CPACK_PACKAGE_VENDOR "Yandex LLC")
    set (CPACK_PACKAGE_DESCRIPTION "ClickHouse ODBC Driver (${ARCH_BITS}-bit)")
    set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "ClickHouse ODBC Driver (${ARCH_BITS}-bit)")
    set (CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/packaging/Readme.rtf")
    set (CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/packaging/License.rtf")
    set (CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ClickHouse/clickhouse-odbc")

    set (CPACK_WIX_ROOT_FEATURE_TITLE "ClickHouse ODBC Driver")
    set (CPACK_WIX_ROOT_FEATURE_DESCRIPTION "ClickHouse ODBC Driver (${ARCH_BITS}-bit)")
    set (CPACK_WIX_PATCH_FILE "${PROJECT_SOURCE_DIR}/packaging/RegConfig.patch.wxs")
    if ("${ARCH_BITS}" STREQUAL "32")
        set (CPACK_WIX_PRODUCT_GUID "9FCA44DB-6963-4EBF-87A6-279331C139EB")
        set (CPACK_WIX_UPGRADE_GUID "3C19591C-7FFC-461A-8828-611EDFBE0619")
    elseif ("${ARCH_BITS}" STREQUAL "64")
        set (CPACK_WIX_PRODUCT_GUID "D77FCBEA-C3A9-442C-8055-83CBDB57009B")
        set (CPACK_WIX_UPGRADE_GUID "B63A7326-E080-49E3-A963-8F5EFC2DDD65")
    endif ()

    include (CPack)
endif ()
