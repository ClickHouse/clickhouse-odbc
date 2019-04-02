find_package (Threads)

add_executable(clickhouse-odbc-ut
    main.cpp
    escape_sequences_ut.cpp
    lexer_ut.cpp
)

target_link_libraries(clickhouse-odbc-ut
    PRIVATE
    clickhouse-odbc-escaping
    gtest-lib
    Threads::Threads
)

add_test(NAME "clickhouse-odbc-ut" COMMAND "clickhouse-odbc-ut")

if (NANOODBC_LIBRARY)
    add_executable(clickhouse-odbc-nano nano.cpp)
    target_link_libraries(clickhouse-odbc-nano PRIVATE ${NANOODBC_LIBRARY})
    add_dependencies(clickhouse-odbc-nano clickhouse-odbc)
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
if (WIN32)
   target_link_libraries(load-ut PRIVATE Iphlpapi)
endif()
if(ODBC_INCLUDE_DIRECTORIES)
    target_include_directories(load-ut PRIVATE ${ODBC_INCLUDE_DIRECTORIES})
endif()
add_test(NAME "load-ut" COMMAND load-ut $<TARGET_FILE:clickhouse-odbc>)
add_test(NAME "load-ut-w" COMMAND load-ut $<TARGET_FILE:clickhouse-odbcw>)
