cmake_minimum_required (VERSION 3.13.5)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

set (CMAKE_MODULE_PATH_orig ${CMAKE_MODULE_PATH})
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
include (cmake/prevent_in_source_builds.cmake)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum OS X deployment version")

project (
    clickhouse-odbc
    VERSION 1.4.1.20250523
    DESCRIPTION "The official ODBC driver implementation for accessing ClickHouse as a data source."
    HOMEPAGE_URL "https://github.com/ClickHouse/clickhouse-odbc"
    LANGUAGES C CXX
)

set (CMAKE_C_STANDARD 11)
set (CMAKE_C_STANDARD_REQUIRED 1)

set (CMAKE_CXX_STANDARD 23)
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

include (cmake/os.cmake)
option(BUILD_TESTING "Build test targets" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

option (CH_ODBC_ALLOW_UNSAFE_DISPATCH "Allow unchecked handle dispatching (may slightly increase performance in some scenarios)" ON)
option (CH_ODBC_ENABLE_SSL "Enable SSL (required for utilizing https:// interface, etc.)" ON)
option (CH_ODBC_ENABLE_INSTALL "Enable install targets (required for packaging)" ON)
cmake_dependent_option (CH_ODBC_ENABLE_TESTING "Enable test targets" ON "BUILD_TESTING" OFF)
option (CH_ODBC_ENABLE_CODE_COVERAGE "Enable Code Coverage" OFF)
cmake_dependent_option (CH_ODBC_USE_ICU "Use ICU library, instead of C++ STD, for Unicode conversions" ON "NOT MSVC" OFF)
option (CH_ODBC_PREFER_BUNDLED_THIRD_PARTIES "Prefer bundled over system variants of third party libraries" ON)
cmake_dependent_option (CH_ODBC_PREFER_BUNDLED_POCO "Prefer bundled over system variants of Poco library" ON "CH_ODBC_PREFER_BUNDLED_THIRD_PARTIES" OFF)
cmake_dependent_option (CH_ODBC_PREFER_BUNDLED_SSL "Prefer bundled over system variants of SSL library" ON "CH_ODBC_PREFER_BUNDLED_POCO" OFF)
cmake_dependent_option (CH_ODBC_PREFER_BUNDLED_FOLLY "Prefer bundled over system variants of Folly library" ON "CH_ODBC_PREFER_BUNDLED_THIRD_PARTIES" OFF)
option (CH_ODBC_PREFER_BUNDLED_ICU "Prefer bundled over system variants of ICU library" ON)
cmake_dependent_option (CH_ODBC_PREFER_BUNDLED_GOOGLETEST "Prefer bundled over system variants of Google Test library" ON "CH_ODBC_PREFER_BUNDLED_THIRD_PARTIES" OFF)
cmake_dependent_option (CH_ODBC_PREFER_BUNDLED_NANODBC "Prefer bundled over system variants of nanodbc library" ON "CH_ODBC_PREFER_BUNDLED_THIRD_PARTIES" OFF)
option (CH_ODBC_RUNTIME_LINK_STATIC "Link with compiler and language runtime statically" OFF)
option (CH_ODBC_THIRD_PARTY_LINK_STATIC "Link with third party libraries statically" ON)

set (CH_ODBC_DEFAULT_DSN_ANSI "ClickHouse DSN (ANSI)" CACHE STRING "Default ANSI DSN name")
set (CH_ODBC_DEFAULT_DSN_UNICODE "ClickHouse DSN (Unicode)" CACHE STRING "Default Unicode DSN name")

if (MSVC)
    # This default encoding mode will be overriden by UNICODE, in the corresponding cases.
    #add_compile_definitions (SBCS _SBCS)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
endif ()

if (OS_FREEBSD)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U_XOPEN_SOURCE -UPOCO_HAVE_FD_EPOLL")
endif ()

add_library(global-libs INTERFACE)
include (cmake/arch.cmake)
include (cmake/sanitize.cmake)

include (GNUInstallDirs)

add_library(global-group INTERFACE)
if (OS_LINUX)
    include(cmake/linux/default_libs.cmake)
    target_link_options(global-group INTERFACE "LINKER:--no-undefined")
elseif(OS_DARWIN)
    include(cmake/darwin/default_libs.cmake)
else()
    link_libraries(global-group)
endif()


# Make sure that all optimized builds have NDEBUG macro set.
foreach (config Release RelWithDebInfo MinSizeRel)
    foreach (lang CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
        string (TOUPPER "${config}" config_uc)
        set (var "${lang}_${config_uc}")
        set (${var} "${${var}} -DNDEBUG")
    endforeach ()
endforeach ()

# Set BUILD_TYPE_* macro for each of build types.
foreach (config Debug Release RelWithDebInfo MinSizeRel)
    foreach (lang CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
        string (TOUPPER "${config}" config_uc)
        set (var "${lang}_${config_uc}")
        set (${var} "${${var}} -DBUILD_TYPE_${config_uc}")
    endforeach ()
endforeach ()

if (CH_ODBC_RUNTIME_LINK_STATIC)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        foreach (config ${CMAKE_CONFIGURATION_TYPES_UC})
            foreach (lang CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
                set (var "${lang}_${config}")
                string (REPLACE "/MD" "/MT" ${var} "${${var}}")
                string (REPLACE "-MD" "-MT" ${var} "${${var}}")
            endforeach ()
        endforeach ()
    endif()
endif ()


if (WERROR)
    # Don't pollute CMAKE_CXX_FLAGS with -Werror as it will break some CMake checks.
    # Instead, adopt modern cmake usage requirement.
    # TODO: Set CMAKE_COMPILE_WARNING_AS_ERROR (cmake 3.24)
    target_compile_options(global-group INTERFACE "-Werror")
endif ()
if (OS_WINDOWS)
    find_package(ODBC REQUIRED)
    find_package(Threads REQUIRED)
endif()
add_subdirectory (contrib EXCLUDE_FROM_ALL)

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

if (NOT CH_ODBC_PREFER_BUNDLED_FOLLY)
    message (WARNING "Folly: using system variant of the library currently not supported")
#   find_package (Folly)
endif ()

if (NOT CH_ODBC_USE_ICU)
    message (WARNING "Not using ICU")
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

set(ENABLE_SSL 1)

add_subdirectory(contrib/poco)


if (CH_ODBC_ENABLE_TESTING)
    include (CTest)
    enable_testing ()
    include (GoogleTest)

    if (NOT TEST_DSN_LIST)
        set (TEST_DSN_LIST "${CH_ODBC_DEFAULT_DSN_ANSI};${CH_ODBC_DEFAULT_DSN_UNICODE}")
    endif ()
endif ()

foreach (config ${CMAKE_CONFIGURATION_TYPES_UC})
    set (CMAKE_${config}_POSTFIX "" CACHE STRING "" FORCE) # Don't append "d" or "md" to output lib name
endforeach ()

set (CMAKE_C_VISIBILITY_PRESET hidden)
set (CMAKE_CXX_VISIBILITY_PRESET hidden)
set (CMAKE_VISIBILITY_INLINES_HIDDEN 1)

include (cmake/packages.cmake)

include (CheckIPOSupported)
check_ipo_supported (RESULT ipo_supported LANGUAGES C CXX)
if (ipo_supported)
    if (NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION) # respect value prodivided by user
        set (CMAKE_INTERPROCEDURAL_OPTIMIZATION 1)
        set (CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG 0)
    endif ()
endif ()

add_subdirectory (driver)


if (CH_ODBC_ENABLE_INSTALL)
    if (NOT WIN32)
        install (
            FILES packaging/odbcinst.ini.sample packaging/odbc.ini.sample
            DESTINATION ${CMAKE_INSTALL_DOCDIR}/config
            COMPONENT Documentation
        )
    endif ()

    install(
        FILES packaging/clickhouse-odbc.tdc.sample
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/config
        COMPONENT Documentation
    )

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
    elseif (UNIX_RHEL OR UNIX_FEDORA OR UNIX_CENTOS)
        set (CPACK_GENERATOR "TGZ;TXZ;RPM")
    elseif (UNIX_DEBIAN OR UNIX_UBUNTU)
        set (CPACK_GENERATOR "TGZ;TXZ") # TODO: add DEB
    else ()
        set (CPACK_GENERATOR "TGZ")
    endif ()

    if (WIN32 AND NOT UNIX)
        set (CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    else ()
        set (CPACK_PACKAGE_INSTALL_DIRECTORY "/usr/local")
    endif ()

    set (CPACK_PROJECT_CONFIG_FILE "${PROJECT_SOURCE_DIR}/CPackLists.txt")

    set (CPACK_PACKAGE_VENDOR "ClickHouse, Inc.")
    set (CPACK_PACKAGE_DESCRIPTION "The official ODBC driver implementation for accessing ClickHouse as a data source.")
    set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "ClickHouse ODBC Driver (${ARCH_BITS}-bit)")
    set (CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/packaging/Readme.rtf")
    set (CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/packaging/License.rtf")
    set (CPACK_PACKAGE_CHECKSUM "SHA256")
    set (CPACK_PACKAGE_RELOCATABLE ON)

    set (CPACK_RPM_COMPONENT_INSTALL OFF)
    set (CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set (CPACK_RPM_PACKAGE_RELEASE_DIST ON)
    set (CPACK_RPM_PACKAGE_LICENSE "ASL 2.0")
    set (CPACK_RPM_PACKAGE_GROUP "Applications/Databases")
    set (CPACK_RPM_COMPRESSION_TYPE "gzip")
    set (CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
    set (CPACK_RPM_CHANGELOG_FILE "${PROJECT_SOURCE_DIR}/packaging/Changelog.txt")

    if ("${ODBC_PROVIDER}" STREQUAL "UnixODBC")
        set (CPACK_RPM_PACKAGE_REQUIRES "unixODBC")
    elseif ("${ODBC_PROVIDER}" STREQUAL "iODBC")
        set (CPACK_RPM_PACKAGE_REQUIRES "libiodbc")
    endif ()

    set (CPACK_RPM_DEBUGINFO_PACKAGE OFF)
    set (CPACK_RPM_PACKAGE_SOURCES OFF)

    set (CPACK_WIX_ROOT_FEATURE_TITLE "ClickHouse ODBC Driver")
    set (CPACK_WIX_ROOT_FEATURE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
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
