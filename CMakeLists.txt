cmake_minimum_required (VERSION 3.13.5)

set (CMAKE_MODULE_PATH_orig ${CMAKE_MODULE_PATH})
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

include (cmake/prevent_in_source_builds.cmake)

project (clickhouse-odbc VERSION 1.1.7.20200414 LANGUAGES C CXX)

set (CMAKE_C_STANDARD 11)
#set (CMAKE_C_EXTENSIONS 0)
set (CMAKE_C_STANDARD_REQUIRED 1)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_EXTENSIONS 0)
set (CMAKE_CXX_STANDARD_REQUIRED 1)

set (CMAKE_POSITION_INDEPENDENT_CODE 1)

set_property (GLOBAL PROPERTY USE_FOLDERS 1)
set (CMAKE_BUILD_COLOR_MAKEFILE 1)
set (CMAKE_INSTALL_DEFAULT_COMPONENT_NAME ANSIDriver)

get_property (IS_MULTI_CONFIG_GENERATOR GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG_GENERATOR)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "RelWithDebInfo")
endif ()

if (NOT CMAKE_CONFIGURATION_TYPES)
    set (CMAKE_CONFIGURATION_TYPES "${CMAKE_BUILD_TYPE}")
endif ()

message (STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
if (IS_MULTI_CONFIG_GENERATOR)
    message (STATUS "CMAKE_CONFIGURATION_TYPES: ${CMAKE_CONFIGURATION_TYPES}")
endif ()

string (TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UC)
string (TOUPPER "${CMAKE_CONFIGURATION_TYPES}" CMAKE_CONFIGURATION_TYPES_UC)

set (CH_ODBC_DEFAULT_DSN_ANSI "ClickHouse DSN (ANSI)" CACHE INTERNAL "Default ANSI DSN")
set (CH_ODBC_DEFAULT_DSN_UNICODE "ClickHouse DSN (Unicode)" CACHE INTERNAL "Default Unicode DSN")
set (CH_ODBC_TEST_DSN_LIST "${CH_ODBC_DEFAULT_DSN_ANSI};${CH_ODBC_DEFAULT_DSN_UNICODE}" CACHE STRING ";-separated list of test DSNs, each test will be executed with each of these DSNs")

include (CMakeDependentOption)

option (BUILD_TESTING "Build test targets" ON)
option (BUILD_SHARED_LIBS "Build shared libraries" OFF)

option (CH_ODBC_ENABLE_INSTALL "Enable install targets (required for packaging)" ON)
option (CH_ODBC_ALLOW_UNSAFE_DISPATCH "Allow unchecked handle dispatching (may slightly increase performance in some scenarios)" ON)

option (CH_ODBC_RUNTIME_LINK_STATIC "Link with compiler and language runtime statically" OFF)
option (CH_ODBC_USE_SYSTEM_THIRD_PARTIES "Use system variants of third party libraries" OFF)
cmake_dependent_option (CH_ODBC_THIRD_PARTY_LINK_STATIC "Link with third party libraries statically" OFF "CH_ODBC_USE_SYSTEM_THIRD_PARTIES" ON)

option (CH_ODBC_USE_ICU "Use ICU library instead of deprecated functionality from C++ <codecvt> standard library for Unicode conversions" ON) # TODO: remove
cmake_dependent_option (CH_ODBC_USE_SYSTEM_ICU "Use system variant of ICU library" ON "CH_ODBC_USE_SYSTEM_THIRD_PARTIES" OFF)
cmake_dependent_option (CH_ODBC_ICU_LINK_STATIC "Link with ICU library statically" ON "CH_ODBC_THIRD_PARTY_LINK_STATIC" OFF)

cmake_dependent_option (CH_ODBC_USE_SYSTEM_POCO "Use system variant of Poco library" ON "CH_ODBC_USE_SYSTEM_THIRD_PARTIES" OFF)
cmake_dependent_option (CH_ODBC_POCO_LINK_STATIC "Link with Poco library statically" ON "CH_ODBC_THIRD_PARTY_LINK_STATIC" OFF)

option (CH_ODBC_ENABLE_SSL "Enable SSL (required for utilizing https:// interface, etc.)" ON) # TODO: remove
cmake_dependent_option (CH_ODBC_USE_SYSTEM_SSL "Use system variant of SSL library" ON "CH_ODBC_USE_SYSTEM_POCO" OFF)
cmake_dependent_option (CH_ODBC_SSL_LINK_STATIC "Link with SSL library statically" ON "CH_ODBC_POCO_LINK_STATIC" OFF)

cmake_dependent_option (CH_ODBC_USE_SYSTEM_FOLLY "Use system variant of Folly library" ON "CH_ODBC_USE_SYSTEM_THIRD_PARTIES" OFF)
cmake_dependent_option (CH_ODBC_FOLLY_LINK_STATIC "Link with Folly library statically" ON "CH_ODBC_THIRD_PARTY_LINK_STATIC" OFF)

cmake_dependent_option (CH_ODBC_USE_SYSTEM_GOOGLETEST "Use system variant of Google Test library" ON "CH_ODBC_USE_SYSTEM_THIRD_PARTIES" OFF)
cmake_dependent_option (CH_ODBC_GOOGLETEST_LINK_STATIC "Link with Google Test library statically" ON "CH_ODBC_THIRD_PARTY_LINK_STATIC" OFF)

cmake_dependent_option (CH_ODBC_USE_SYSTEM_NANODBC "Use system variant of nanodbc library" ON "CH_ODBC_USE_SYSTEM_THIRD_PARTIES" OFF)
cmake_dependent_option (CH_ODBC_NANODBC_LINK_STATIC "Link with nanodbc library statically" ON "CH_ODBC_THIRD_PARTY_LINK_STATIC" OFF)

if (MSVC)
    # This default encoding mode will be overriden by UNICODE, in the corresponding cases.
    add_compile_definitions (SBCS _SBCS)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
endif ()

if (ARCH_FREEBSD)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U_XOPEN_SOURCE -UPOCO_HAVE_FD_EPOLL")
endif ()

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
    elseif (
        CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
        (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT APPLE)
    )
        if (NOT APPLE AND NOT WIN32 AND NOT ARCH_FREEBSD)
            set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--whole-archive -static-libgcc -static-libstdc++ -Wl,--no-whole-archive")
            set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--whole-archive -static-libgcc -static-libstdc++ -Wl,--no-whole-archive")
        else ()
            set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
            set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        endif ()
    else ()
        message (FATAL_ERROR "linking with compiler and language runtime statically is not supported in the current build environment")
    endif()
endif ()

include (GNUInstallDirs)
include (cmake/arch.cmake)
include (cmake/sanitize.cmake)
include (cmake/conan/conan.cmake)

set (CONAN_REQUIRES_LIST)
set (CONAN_OPTIONS_LIST)
set (CONAN_SETTINGS_LIST)
set (CONAN_THIRD_PARTY_LIBS_TO_REDISTRIBUTE)

list (APPEND CONAN_SETTINGS_LIST "compiler.cppstd=${CMAKE_CXX_STANDARD}")

if (CH_ODBC_RUNTIME_LINK_STATIC)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        list (APPEND CONAN_SETTINGS_LIST "compiler.runtime=MT")
    else ()
        list (APPEND CONAN_SETTINGS_LIST "compiler.runtime=MD")
    endif ()
endif ()

find_package (Threads REQUIRED)
find_package (ODBC REQUIRED)

if (CH_ODBC_USE_ICU)
    if (CH_ODBC_USE_SYSTEM_ICU)
        if (CH_ODBC_ICU_LINK_STATIC)
            message (FATAL_ERROR "ICU: using static system variant of the library currently not supported")
        endif ()
    else ()
        list (APPEND CONAN_REQUIRES_LIST "icu/64.2")
        if (CH_ODBC_ICU_LINK_STATIC)
            list (APPEND CONAN_OPTIONS_LIST "icu:shared=False")
        else ()
            list (APPEND CONAN_OPTIONS_LIST "icu:shared=True")
            list (APPEND CONAN_THIRD_PARTY_LIBS_TO_REDISTRIBUTE "$<TARGET_FILE:ICU::uc>")
            list (APPEND CONAN_THIRD_PARTY_LIBS_TO_REDISTRIBUTE "$<TARGET_FILE:ICU::dt>")
        endif ()
    endif ()
endif ()

if (CH_ODBC_ENABLE_SSL)
    if (CH_ODBC_USE_SYSTEM_SSL)
        if (CH_ODBC_SSL_LINK_STATIC)
            set (OPENSSL_USE_STATIC_LIBS TRUE)
        endif ()

        if (CH_ODBC_RUNTIME_LINK_STATIC)
            set (OPENSSL_MSVC_STATIC_RT TRUE)
        endif ()

        find_package (OpenSSL REQUIRED)
    endif ()
endif ()

if (CH_ODBC_USE_SYSTEM_POCO)
    find_package (Poco REQUIRED COMPONENTS Foundation Net NetSSL)
endif ()

if (CH_ODBC_USE_SYSTEM_FOLLY)
    message (FATAL_ERROR "Folly: using system variant of the library currently not supported")
#   find_package (Folly REQUIRED)
endif ()

if (BUILD_TESTING)
    if (CH_ODBC_RUNTIME_LINK_STATIC)
        set (GTEST_MSVC_SEARCH "MT")
    else ()
        set (GTEST_MSVC_SEARCH "MD")
    endif ()

    if (CH_ODBC_USE_SYSTEM_GOOGLETEST)
        find_package (GTest REQUIRED)
    endif ()

    if (CH_ODBC_USE_SYSTEM_NANODBC)
        message (FATAL_ERROR "nanodbc: using system variant of the library currently not supported")
#       find_package (nanodbc REQUIRED)
    endif ()
endif ()

if (CONAN_REQUIRES_LIST)
    conan_check (VERSION 1.24.0 REQUIRED)
    if (IS_MULTI_CONFIG_GENERATOR)
        conan_cmake_run (
            REQUIRES "${CONAN_REQUIRES_LIST}"
            OPTIONS "${CONAN_OPTIONS_LIST}"
            SETTINGS "${CONAN_SETTINGS_LIST}"
            CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}"
            GENERATORS cmake_paths
            UPDATE
            BUILD missing
        )
    else ()
        conan_cmake_run (
            REQUIRES "${CONAN_REQUIRES_LIST}"
            OPTIONS "${CONAN_OPTIONS_LIST}"
            SETTINGS "${CONAN_SETTINGS_LIST}"
            BUILD_TYPE "${CMAKE_BUILD_TYPE}"
            GENERATORS cmake_paths
            UPDATE
            BUILD missing
        )
    endif ()
endif ()

include("${CMAKE_BINARY_DIR}/conan_paths.cmake")

if (CONAN_ICU_ROOT)
    set (ICU_ROOT "${CONAN_ICU_ROOT}")
endif ()

find_package (ICU COMPONENTS uc dt REQUIRED)

add_subdirectory (contrib EXCLUDE_FROM_ALL)

if (BUILD_TESTING)
    include (CTest)
    enable_testing ()
    include (GoogleTest)
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
    if (NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION) # respect value prodivided by user
        set (CMAKE_INTERPROCEDURAL_OPTIMIZATION 1)
        set (CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG 0)
    endif ()
endif ()

add_subdirectory (driver)
if (BUILD_TESTING)
    add_subdirectory (test)
endif ()

if (CH_ODBC_ENABLE_INSTALL)
    if (NOT "${ODBC_PROVIDER}" STREQUAL "MDAC")
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

    if (CONAN_THIRD_PARTY_LIBS_TO_REDISTRIBUTE)
        install (
            FILES "${CONAN_THIRD_PARTY_LIBS_TO_REDISTRIBUTE}"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT ThirdPartyLibraries
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
