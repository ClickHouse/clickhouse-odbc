function (declare_odbc_test_targets libname UNICODE)
    if (UNICODE)
        set (WIDECHAR_SUFFIX w)
    else ()
        set (WIDECHAR_SUFFIX)
    endif ()

    add_executable (${libname}-ut
        gtest_main.cpp
        gtest_env.h
        gtest_env.cpp
        common_utils.h
        utils_ut.cpp
        escape_sequences_ut.cpp
        lexer_ut.cpp
        AttributeContainer_ut.cpp
        type_conversion_ut.cpp
        buffer_filling_ut.cpp
        connection_string_ut.cpp
        performance_ut.cpp
        statement_parameter_binding_ut.cpp
    )

    target_link_libraries (${libname}-ut
        PRIVATE ${libname}-impl
        PRIVATE gtest
    )

    add_test (NAME ${libname}-ut COMMAND ${libname}-ut)

    add_executable (${libname}-load-ut
        load_ut.cpp
    )

    target_link_libraries (${libname}-load-ut
        PRIVATE Poco::Foundation
        PRIVATE Threads::Threads
    )
    if (OS_DARWIN OR OS_LINUX)
        target_link_libraries (${libname}-load-ut
            PRIVATE ch_contrib::unixodbc
        )
    elseif (OS_WINDOWS)
        target_link_libraries (${libname}-load-ut
            PRIVATE ODBC::App
        )
    endif()

    target_include_directories (${libname}-load-ut
        PRIVATE ${PROJECT_BINARY_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}
    )

    if (UNICODE)
        target_compile_definitions (${libname}-load-ut
            PUBLIC UNICODE
            PUBLIC _UNICODE
        )
    endif ()

    add_test (NAME "${libname}-load-ut" COMMAND ${libname}-load-ut $<TARGET_FILE:clickhouse-odbc>)
    add_test (NAME "${libname}-load-ut-w" COMMAND ${libname}-load-ut $<TARGET_FILE:clickhouse-odbcw>)

    add_executable (${libname}-client-it
        gtest_main.cpp
        gtest_env.h
        gtest_env.cpp
        common_utils.h
        client_utils.h
        client_test_base.h
        ${PROJECT_SOURCE_DIR}/driver/utils/type_info.cpp
        ${PROJECT_SOURCE_DIR}/driver/utils/unicode_converter.cpp
        ${PROJECT_SOURCE_DIR}/driver/utils/conversion_context.cpp
        ${PROJECT_SOURCE_DIR}/driver/utils/conversion.h
        ${PROJECT_SOURCE_DIR}/driver/utils/conversion_std.h
        ${PROJECT_SOURCE_DIR}/driver/utils/conversion_icu.h
        misc_it.cpp
        datetime_it.cpp
        column_bindings_it.cpp
        statement_parameter_bindings_it.cpp
        statement_parameters_it.cpp
        performance_it.cpp
        type_info_it.cpp
    )

    target_link_libraries (${libname}-client-it
        PRIVATE Poco::Foundation
        PRIVATE gtest
        PRIVATE gmock
        PRIVATE Threads::Threads
        PRIVATE ${libname}-impl
    )

    if (OS_LINUX OR OS_DARWIN)
        target_link_libraries (${libname}-impl PUBLIC ch_contrib::unixodbc ch_contrib::nanodbc)
    else()
        target_link_libraries (${libname}-impl PUBLIC ODBC::App)
    endif()

    if (CH_ODBC_USE_ICU)
        target_include_directories (${libname}-client-it
            PUBLIC ch_contrib::icu
        )

        target_link_libraries (${libname}-client-it
            PUBLIC ch_contrib::icu
        )
    endif ()

    target_include_directories (${libname}-client-it
        PRIVATE ${FOLLY_INLUDE_DIR}      # TODO: switch to linking with folly lib target?
        PRIVATE ${PROJECT_BINARY_DIR}
        PRIVATE ${PROJECT_SOURCE_DIR}
    )

    if (UNICODE)
        target_compile_definitions (${libname}-client-it
            PUBLIC UNICODE
            PUBLIC _UNICODE
        )
    endif ()

    set(test_dsn_counter 0)
    foreach (test_dsn ${TEST_DSN_LIST})
        if (test_dsn)
            add_test (NAME ${libname}-client-it-dsn-${test_dsn_counter} COMMAND ${libname}-client-it "${test_dsn}")
            math (EXPR test_dsn_counter "${test_dsn_counter}+1")
        endif ()
    endforeach ()

    if (TARGET nanodbc
#        AND (
#            (NANODBC_ENABLE_UNICODE AND UNICODE) OR
#            (NOT NANODBC_ENABLE_UNICODE AND NOT UNICODE)
#        )
    )
        if (NANODBC_ENABLE_UNICODE)
            set (nanoname nanow)
        else ()
            set (nanoname nano)
        endif ()

        add_executable (${libname}-${nanoname}-it
            nano_it.cpp
            ${PROJECT_BINARY_DIR}/driver/platform/config_cmake${WIDECHAR_SUFFIX}.h
        )

        target_link_libraries (${libname}-${nanoname}-it
            PRIVATE nanodbc
        )

        target_include_directories (${libname}-${nanoname}-it
            PRIVATE ${PROJECT_BINARY_DIR} # for driver/platform/config_cmake${WIDECHAR_SUFFIX}.h
        )

        if (UNICODE)
            target_compile_definitions (${libname}-${nanoname}-it
                PUBLIC UNICODE
                PUBLIC _UNICODE
            )
        endif ()

        set(test_dsn_counter 0)
        foreach (test_dsn ${TEST_DSN_LIST})
            if (test_dsn)
                add_test (NAME ${libname}-${nanoname}-it-dsn-${test_dsn_counter} COMMAND ${libname}-${nanoname}-it "DSN=${test_dsn}")

                if (NOT (CMAKE_BUILD_TYPE_UC STREQUAL "TSAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "ASAN" OR CMAKE_BUILD_TYPE_UC STREQUAL "MSAN"))
                    #TODO: add_test (NAME "${libname}-${nanoname}-valgrind-it-dsn-${test_dsn_counter} COMMAND valgrind -v --leak-check=full --show-leak-kinds=all $<TARGET_FILE:${libname}-${nanoname}-it> "DSN=${test_dsn}")
                endif ()

                # gdb -ex "catch throw" -ex run --args ./clickhouse-odbc-nano DSN=clickhouse_localhost_system

                math (EXPR test_dsn_counter "${test_dsn_counter}+1")
            endif ()
        endforeach ()
    endif ()
endfunction ()

declare_odbc_test_targets (clickhouse-odbc 0)
declare_odbc_test_targets (clickhouse-odbcw 1)
