if (CLICKHOUSE_ODBC_TEST)
    add_subdirectory (gtest)
endif ()

if (USE_INTERNAL_POCO_LIBRARY)
    add_subdirectory (poco)
endif ()
