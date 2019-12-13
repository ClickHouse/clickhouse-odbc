function(declare_odbc_test_targets libname UNICODE)
    if (UNICODE)
        set(WIDECHAR_SUFFIX w)
    else ()
        set(WIDECHAR_SUFFIX)
    endif ()

    add_executable(${libname}-ut
        gtest_main.cpp
        gtest_env.h
        gtest_env.cpp
        escape_sequences_ut.cpp
        lexer_ut.cpp
        AttributeContainer_ut.cpp
        type_conversion_ut.cpp
        buffer_filling_ut.cpp
        connection_string_ut.cpp
    )

    target_link_libraries(${libname}-ut
        PRIVATE ${libname}-impl
        PRIVATE gtest
    )

    add_test(NAME ${libname}-ut COMMAND ${libname}-ut)

    add_executable(${libname}-load-ut
        load_ut.cpp
    )

    target_link_libraries(${libname}-load-ut
        PRIVATE ODBC::API
        PRIVATE ${Poco_Foundation_LIBRARY}
        PRIVATE ${CMAKE_DL_LIBS}
        PRIVATE Threads::Threads
    )

    target_include_directories(${libname}-load-ut
        PRIVATE ${PROJECT_BINARY_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${Poco_INCLUDE_DIRS}
    )

    if (UNICODE)
        target_compile_definitions(${libname}-load-ut
            PUBLIC UNICODE
            PUBLIC _UNICODE
        )
    endif ()

    add_test(NAME "${libname}-load-ut" COMMAND ${libname}-load-ut $<TARGET_FILE:clickhouse-odbc>)
    add_test(NAME "${libname}-load-ut-w" COMMAND ${libname}-load-ut $<TARGET_FILE:clickhouse-odbcw>)

    add_executable(${libname}-client-it
        gtest_main.cpp
        gtest_env.h
        gtest_env.cpp
        client_utils.h
        client_test_base.h
        ${PROJECT_SOURCE_DIR}/driver/type_info.cpp
        ${PROJECT_SOURCE_DIR}/driver/utils/unicode_conv.h
        misc_it.cpp
        statement_parameters_it.cpp
        performance_it.cpp
    )

    target_link_libraries(${libname}-client-it
        PRIVATE ODBC::API
        PRIVATE ${Poco_Foundation_LIBRARY}
        PRIVATE gtest
        PRIVATE ${CMAKE_DL_LIBS}
        PRIVATE Threads::Threads
    )

    target_include_directories(${libname}-client-it
        PRIVATE ${PROJECT_BINARY_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}
        PRIVATE ${Poco_INCLUDE_DIRS}
    )

    if (UNICODE)
        target_compile_definitions(${libname}-client-it
            PUBLIC UNICODE
            PUBLIC _UNICODE
        )
    endif ()

    if (TEST_DSN)
        add_test(NAME ${libname}-client-it COMMAND ${libname}-client-it ${TEST_DSN})
    endif ()

    if (TEST_DSN_W)
        add_test(NAME ${libname}-client-it-w COMMAND ${libname}-client-it ${TEST_DSN_W})
    endif ()

    if (NANOODBC_LIBRARY
#        AND (
#            (NANODBC_ENABLE_UNICODE AND UNICODE) OR
#            (NOT NANODBC_ENABLE_UNICODE AND NOT UNICODE)
#        )
    )
        if (NANODBC_ENABLE_UNICODE)
            set(nanoname nanow)
        else ()
            set(nanoname nano)
        endif()

        add_executable(${libname}-${nanoname}-it
            nano_it.cpp
            ${PROJECT_BINARY_DIR}/driver/platform/config_cmake${WIDECHAR_SUFFIX}.h
        )

        target_link_libraries(${libname}-${nanoname}-it
            PRIVATE ${NANOODBC_LIBRARY}
        )

        target_include_directories(${libname}-${nanoname}-it
            PRIVATE ${PROJECT_BINARY_DIR} # for driver/platform/config_cmake${WIDECHAR_SUFFIX}.h
        )

        if (UNICODE)
            target_compile_definitions(${libname}-${nanoname}-it
                PUBLIC UNICODE
                PUBLIC _UNICODE
            )
        endif ()

        if (TEST_DSN)
            add_test(NAME ${libname}-${nanoname}-it COMMAND ${libname}-${nanoname}-it DSN=${TEST_DSN})
            if (NOT (CMAKE_BUILD_TYPE_UC STREQUAL "TSAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "ASAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "MSAN"))
                #TODO: add_test(NAME "${libname}-${nanoname}-valgrind-it" COMMAND valgrind -v --leak-check=full --show-leak-kinds=all $<TARGET_FILE:${libname}-${nanoname}-it> DSN=${TEST_DSN})
            endif ()
        endif ()

        if (TEST_DSN_W)
            add_test(NAME ${libname}-${nanoname}-it-w COMMAND ${libname}-${nanoname}-it DSN=${TEST_DSN_W})
            if (NOT (CMAKE_BUILD_TYPE_UC STREQUAL "TSAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "ASAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "MSAN"))
                #TODO: add_test(NAME "${libname}-${nanoname}-valgrind-it-w" COMMAND valgrind -v --leak-check=full --show-leak-kinds=all $<TARGET_FILE:${libname}-${nanoname}-it> DSN=${TEST_DSN_W})
            endif ()
        endif ()
        # gdb -ex "catch throw" -ex run --args ./clickhouse-odbc-nano DSN=clickhouse_localhost_system
    endif ()
endfunction()

declare_odbc_test_targets(clickhouse-odbc 0)
declare_odbc_test_targets(clickhouse-odbcw 1)
