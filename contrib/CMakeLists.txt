if (CLICKHOUSE_ODBC_TEST)
    add_subdirectory (gtest)
    if (USE_INTERNAL_NANOODBC_LIBRARY)
        # git clone --recursive https://github.com/lexicalunit/nanodbc.git
        set (NANODBC_DISABLE_TESTS 1 CACHE INTERNAL "")
        set (NANODBC_ENABLE_UNICODE ${UNICODE} CACHE INTERNAL "")
        add_subdirectory (nanodbc)
    endif ()
endif ()

if (USE_INTERNAL_SSL_LIBRARY)
    set (BUILD_SHARED 0)
    set (USE_SHARED 0)
    add_subdirectory (ssl)
endif ()

if (USE_INTERNAL_POCO_LIBRARY)
    set (POCO_UNBUNDLED_ZLIB 1 CACHE INTERNAL "")
    add_subdirectory (poco)
    if (OPENSSL_FOUND AND TARGET Crypto)
        # Bug in poco https://github.com/pocoproject/poco/pull/2100 found on macos
        target_include_directories(Crypto PUBLIC $<BUILD_INTERFACE:${OPENSSL_INCLUDE_DIR}>)
    endif ()
    if (USE_INTERNAL_POCO_LIBRARY)
        # Remove pthread from linking poco's Net
        set_property(TARGET Net PROPERTY INTERFACE_LINK_LIBRARIES "")
        set_property(TARGET Foundation PROPERTY INTERFACE_LINK_LIBRARIES "")
    endif ()
endif ()
