if (CLICKHOUSE_ODBC_TEST)
    add_subdirectory (gtest)
endif ()

if (USE_INTERNAL_POCO_LIBRARY)
    add_subdirectory (poco)
    if (OPENSSL_FOUND AND TARGET Crypto)
        # Bug in poco https://github.com/pocoproject/poco/pull/2100 found on macos
        target_include_directories(Crypto PUBLIC ${OPENSSL_INCLUDE_DIR})
    endif ()
endif ()
