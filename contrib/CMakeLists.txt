get_filename_component (_current_dir_name "${CMAKE_CURRENT_LIST_DIR}" NAME)
if (CMAKE_FOLDER)
    set (CMAKE_FOLDER "$CMAKE_FOLDER/${_current_dir_name}")
else ()
    set (CMAKE_FOLDER "${_current_dir_name}")
endif ()
unset (_current_dir_name)

if (CH_ODBC_ENABLE_SSL AND NOT MSVC)
    if (NOT CH_ODBC_USE_SYSTEM_SSL OR NOT OPENSSL_FOUND)
        set (USE_SHARED 0)

        if (XCODE)
            set (ENABLE_ASM 0 CACHE INTERNAL "")
        endif ()

        add_subdirectory (ssl)

        # Mimic FindOpenSSL for Poco to pickup.
        set (OPENSSL_FOUND TRUE)
        set (OPENSSL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ssl/include")
        set (OPENSSL_CRYPTO_LIBRARY crypto)
        set (OPENSSL_CRYPTO_LIBRARIES ${OPENSSL_LIBS})
        set (OPENSSL_SSL_LIBRARY ssl)
        set (OPENSSL_SSL_LIBRARIES ${OPENSSL_LIBS})
        set (OPENSSL_LIBRARIES ${OPENSSL_LIBS})
    endif ()
endif ()

if (NOT CH_ODBC_USE_SYSTEM_POCO OR NOT Poco_FOUND)
    if (CH_ODBC_ENABLE_SSL AND MSVC)
        set (ENABLE_NETSSL_WIN ON CACHE INTERNAL "")
    endif ()

    if (CH_ODBC_THIRD_PARTY_LINK_STATIC)
        set (POCO_STATIC ON CACHE INTERNAL "")
    endif ()

    if (CH_ODBC_RUNTIME_LINK_STATIC)
        set (POCO_MT ON CACHE INTERNAL "")
    endif ()

    set (CMAKE_MODULE_PATH_bak ${CMAKE_MODULE_PATH})
    set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH_orig})

    add_subdirectory (poco)

    set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH_bak})
    unset (CMAKE_MODULE_PATH_bak)

    if (TARGET NetSSL)
        add_library(Poco::NetSSL ALIAS NetSSL)
    endif ()

    if (TARGET NetSSLWin)
        add_library(Poco::NetSSLWin ALIAS NetSSLWin)
    endif ()

    add_library(Poco::Net ALIAS Net)
    add_library(Poco::Foundation ALIAS Foundation)
    add_library(Poco::Util ALIAS Util)

    # Remove pthread from linking poco's Net
    set_property (TARGET Net PROPERTY INTERFACE_LINK_LIBRARIES "")
    set_property (TARGET Foundation PROPERTY INTERFACE_LINK_LIBRARIES "")
endif ()

# Bug in poco https://github.com/pocoproject/poco/pull/2100 found on macos
if (OPENSSL_FOUND)
    if (TARGET Crypto)
        target_include_directories (Crypto INTERFACE $<BUILD_INTERFACE:${OPENSSL_INCLUDE_DIR}>)
    elseif (TARGET Poco::Crypto)
        target_include_directories (Poco::Crypto INTERFACE $<BUILD_INTERFACE:${OPENSSL_INCLUDE_DIR}>)
    endif ()
endif()

if (MSVC)
    if (TARGET Foundation)
        set_property(TARGET Foundation APPEND PROPERTY INTERFACE_LINK_LIBRARIES Iphlpapi)
    elseif (TARGET Poco::Foundation)
        set_property(TARGET Poco::Foundation APPEND PROPERTY INTERFACE_LINK_LIBRARIES Iphlpapi)
    endif ()
endif ()

# if (NOT CH_ODBC_USE_SYSTEM_FOLLY OR NOT Folly_FOUND)
#    add_subdirectory (folly)
    set (FOLLY_INLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/folly" CACHE INTERNAL "")
# endif ()

if (BUILD_TESTING)
    if (NOT CH_ODBC_USE_SYSTEM_GOOGLETEST OR NOT GTEST_FOUND)
        if (NOT CH_ODBC_RUNTIME_LINK_STATIC)
            set (gtest_force_shared_crt ON CACHE INTERNAL "")
        endif ()

        set (gtest_hide_internal_symbols ON CACHE INTERNAL "")
        set (BUILD_GMOCK   ON CACHE BOOL "")
        set (INSTALL_GTEST OFF CACHE BOOL "")

        add_subdirectory (googletest)

        mark_as_advanced (BUILD_GMOCK)
        mark_as_advanced (INSTALL_GTEST)

        set_target_properties (gtest      PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
        set_target_properties (gtest_main PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
        set_target_properties (gmock      PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
        set_target_properties (gmock_main PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
    endif ()

#   if (NOT CH_ODBC_USE_SYSTEM_NANODBC OR NOT NANODBC_FOUND)

        # Todo: test and fix in unicode iodbc

        set (NANODBC_DISABLE_EXAMPLES 1 CACHE INTERNAL "")
        set (NANODBC_DISABLE_LIBCXX   1 CACHE INTERNAL "")
        set (NANODBC_DISABLE_TESTS    1 CACHE INTERNAL "")
        set (NANODBC_DISABLE_INSTALL  1 CACHE INTERNAL "")
        set (NANODBC_ENABLE_UNICODE   0 CACHE INTERNAL "")

        add_subdirectory (nanodbc)

        set_target_properties (nanodbc PROPERTIES INTERFACE_LINK_LIBRARIES ODBC::App)
#   endif ()
endif ()

# Some of the above may define this target.
if (TARGET uninstall AND CMAKE_FOLDER)
    set_target_properties (uninstall PROPERTIES FOLDER "${CMAKE_FOLDER}")
endif ()

# Some of the above may set a cache entry that will interfere with later reconfigurations.
unset (ODBC_CONFIG CACHE)
