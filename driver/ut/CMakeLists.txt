function(declare_odbc_ut_targets exename UNICODE)
    if (UNICODE)
        set (odbc_lib clickhouse-odbcw_static)
    else()
        set (odbc_lib clickhouse-odbc_static)
    endif()

    add_executable(${exename}
        main.cpp
        escape_sequences_ut.cpp
        lexer_ut.cpp
        AttributeContainer_test.cpp
    )

    target_link_libraries(${exename}
        PRIVATE clickhouse-odbc-escaping
        PRIVATE ${odbc_lib}
        PRIVATE gtest-lib
        PRIVATE Threads::Threads
    )

    add_test(NAME ${exename} COMMAND ${exename})
endfunction()

declare_odbc_ut_targets(clickhouse-odbc-ut 0)
declare_odbc_ut_targets(clickhouse-odbcw-ut 1)

if (NANOODBC_LIBRARY)
    add_executable(clickhouse-odbc-nano nano.cpp)

    target_link_libraries(clickhouse-odbc-nano PRIVATE ${NANOODBC_LIBRARY})

    if (TEST_DSN)
        add_test(NAME "clickhouse-odbc-nano" COMMAND clickhouse-odbc-nano DSN=${TEST_DSN})
        if (NOT (CMAKE_BUILD_TYPE_UC STREQUAL "TSAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "ASAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "MSAN"))
            #TODO: add_test(NAME "clickhouse-odbc-nano-valgrind" COMMAND valgrind -v --leak-check=full --show-leak-kinds=all ${CMAKE_CURRENT_BINARY_DIR}/clickhouse-odbc-nano DSN=${TEST_DSN})
        endif ()
    endif ()

    if (TEST_DSN_W)
        add_test(NAME "clickhouse-odbc-nano-w" COMMAND clickhouse-odbc-nano DSN=${TEST_DSN_W})
        if (NOT (CMAKE_BUILD_TYPE_UC STREQUAL "TSAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "ASAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "MSAN"))
            #TODO: add_test(NAME "clickhouse-odbc-nano-valgrind-w" COMMAND valgrind -v --leak-check=full --show-leak-kinds=all ${CMAKE_CURRENT_BINARY_DIR}/clickhouse-odbc-nano DSN=${TEST_DSN_W})
        endif ()
    endif ()
    # gdb -ex "catch throw" -ex run --args ./clickhouse-odbc-nano DSN=clickhouse_localhost_system
endif ()

add_executable(load-ut load.cpp)

target_link_libraries(load-ut PRIVATE ${Poco_Foundation_LIBRARY} ${CMAKE_DL_LIBS} Threads::Threads)

target_include_directories(load-ut PRIVATE ${Poco_INCLUDE_DIRS})
target_include_directories(load-ut PRIVATE ${ODBC_INCLUDE_DIRECTORIES})

if (WIN32)
   target_link_libraries(load-ut PRIVATE Iphlpapi)
endif()

add_test(NAME "load-ut" COMMAND load-ut $<TARGET_FILE:clickhouse-odbc>)
add_test(NAME "load-ut-w" COMMAND load-ut $<TARGET_FILE:clickhouse-odbcw>)
