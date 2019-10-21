function(declare_odbc_test_targets libname UNICODE)
    add_executable(${libname}-ut
        gtest_main.cpp
        escape_sequences_ut.cpp
        lexer_ut.cpp
        AttributeContainer_ut.cpp
    )

    target_link_libraries(${libname}-ut
        PRIVATE clickhouse-odbc-escaping
        PRIVATE ${libname}_static
        PRIVATE gtest-lib
        PRIVATE Threads::Threads
    )

    add_test(NAME ${libname}-ut COMMAND ${libname}-ut)

    add_executable(${libname}-load-ut
        load.cpp
    )

    target_link_libraries(${libname}-load-ut
        PRIVATE ${ODBC_LIBRARIES}
        PRIVATE ${Poco_Foundation_LIBRARY}
        PRIVATE ${CMAKE_DL_LIBS}
        PRIVATE Threads::Threads
    )

    target_include_directories(${libname}-load-ut
        PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
        PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/..
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
        PRIVATE ${Poco_INCLUDE_DIRS}
        PRIVATE ${ODBC_INCLUDE_DIRECTORIES}
    )

    if (WIN32)
        target_link_libraries(${libname}-load-ut PRIVATE Iphlpapi)
    endif ()

    add_test(NAME "${libname}-load-ut" COMMAND ${libname}-load-ut $<TARGET_FILE:clickhouse-odbc>)
    add_test(NAME "${libname}-load-ut-w" COMMAND ${libname}-load-ut $<TARGET_FILE:clickhouse-odbcw>)

    add_executable(${libname}-client-test
        gtest_main.cpp
        statement_parameters_guid.cpp
    )

    target_link_libraries(${libname}-client-test
        PRIVATE ${ODBC_LIBRARIES}
        PRIVATE ${Poco_Foundation_LIBRARY}
        PRIVATE gtest-lib
        PRIVATE ${CMAKE_DL_LIBS}
        PRIVATE Threads::Threads
    )

    target_include_directories(${libname}-client-test
        PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
        PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/..
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
        PRIVATE ${Poco_INCLUDE_DIRS}
        PRIVATE ${ODBC_INCLUDE_DIRECTORIES}
    )

    add_test(NAME ${libname}-client-test COMMAND ${libname}-client-test)

    if (NANOODBC_LIBRARY
#        AND (
#            (NANODBC_ENABLE_UNICODE AND UNICODE) OR
#            (NOT NANODBC_ENABLE_UNICODE AND NOT UNICODE)
#        )
    )
        if (NANODBC_ENABLE_UNICODE)
            set(nanoname nano)
        else ()
            set(nanoname nanow)
        endif()

        add_executable(${libname}-${nanoname}
            nano.cpp
        )

        target_link_libraries(${libname}-${nanoname}
            PRIVATE ${NANOODBC_LIBRARY}
        )

        if (TEST_DSN)
            add_test(NAME "${libname}-${nanoname}" COMMAND ${libname}-${nanoname} DSN=${TEST_DSN})
            if (NOT (CMAKE_BUILD_TYPE_UC STREQUAL "TSAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "ASAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "MSAN"))
                #TODO: add_test(NAME "${libname}-${nanoname}-valgrind" COMMAND valgrind -v --leak-check=full --show-leak-kinds=all $<TARGET_FILE:${libname}-${nanoname}> DSN=${TEST_DSN})
            endif ()
        endif ()

        if (TEST_DSN_W)
            add_test(NAME "${libname}-${nanoname}-w" COMMAND ${libname}-${nanoname} DSN=${TEST_DSN_W})
            if (NOT (CMAKE_BUILD_TYPE_UC STREQUAL "TSAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "ASAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "MSAN"))
                #TODO: add_test(NAME "${libname}-${nanoname}-valgrind-w" COMMAND valgrind -v --leak-check=full --show-leak-kinds=all $<TARGET_FILE:${libname}-${nanoname}> DSN=${TEST_DSN_W})
            endif ()
        endif ()
        # gdb -ex "catch throw" -ex run --args ./clickhouse-odbc-nano DSN=clickhouse_localhost_system
    endif ()
endfunction()

declare_odbc_test_targets(clickhouse-odbc 0)
declare_odbc_test_targets(clickhouse-odbcw 1)
