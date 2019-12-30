function(declare_odbc_lib_targets libname UNICODE)

# See the usage of each workaround in the code, for comments.
unset(WORKAROUND_ENABLE_TRIM_TRAILING_NULL)
unset(WORKAROUND_ENABLE_DEFINE_SQLBindParam)
unset(WORKAROUND_ENABLE_NO_UNICODE_CHARS_IN_COLUMN_NAMES_IN_TESTS)

if (UNICODE)
    set(DRIVER_PREFIX Unicode)
    set(WIDECHAR_SUFFIX w)
else ()
    set(DRIVER_PREFIX ANSI)
    set(WIDECHAR_SUFFIX)
    if ("${ODBC_PROVIDER}" STREQUAL "UnixODBC")
        set(WORKAROUND_ENABLE_TRIM_TRAILING_NULL 1)
    endif ()
endif ()

if ("${ODBC_PROVIDER}" STREQUAL "iODBC")
    set(WORKAROUND_ENABLE_DEFINE_SQLBindParam 1)
    set(WORKAROUND_ENABLE_NO_UNICODE_CHARS_IN_COLUMN_NAMES_IN_TESTS 1)
endif ()

message (STATUS "${libname}: ${DRIVER_PREFIX}")

configure_file (
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/config_cmake.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/platform/config_cmake${WIDECHAR_SUFFIX}.h
)

unset(WORKAROUND_ENABLE_TRIM_TRAILING_NULL)
unset(WORKAROUND_ENABLE_DEFINE_SQLBindParam)
unset(WORKAROUND_ENABLE_NO_UNICODE_CHARS_IN_COLUMN_NAMES_IN_TESTS)

# In order to enable testing, put every non-public symbol to a static library (which is then used by shared library and unit-test binary).
add_library(${libname}-impl STATIC
    utils/read_helpers.cpp
    utils/type_parser.cpp

    config/config.cpp

    escaping/escape_sequences.cpp
    escaping/lexer.cpp

    attributes.cpp
    connection.cpp
    descriptor.cpp
    diagnostics.cpp
    driver.cpp
    environment.cpp
    exception.cpp
    object.cpp
    result_set.cpp
    statement.cpp
    type_info.cpp


    ${CMAKE_CURRENT_BINARY_DIR}/platform/config_cmake${WIDECHAR_SUFFIX}.h
    platform/platform.h

    utils/utils.h
    utils/unicode_conv.h
    utils/iostream_debug_helpers.h
    utils/read_helpers.h
    utils/type_parser.h
    utils/scope_guard.h

    config/config.h
    config/ini_defines.h

    escaping/escape_sequences.h
    escaping/lexer.h
    escaping/lexer_declare.h
    escaping/function_declare.h

    attributes.h
    connection.h
    descriptor.h
    diagnostics.h
    driver.h
    environment.h
    exception.h
    object.h
    result_set.h
    statement.h
    type_info.h
)

set (WIN_SOURCES)
if (WIN32)
    set (WIN_SOURCES
        platform/win/dllmain.cpp
        platform/win/resource.h
        platform/win/setup.cpp
        platform/win/resource.rc
    )

    if (NOT "${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
        # An entry in .def file still must be added for each exported function for 32-bit targets.
        # See also the definition of 'EXPORTED_FUNCTION' C/C++ macro in the code.
        set (WIN_SOURCES
            ${WIN_SOURCES}
            platform/win/${libname}.def
        )
    endif()
endif()

if (UNICODE)
    target_compile_definitions(${libname}-impl
        PUBLIC UNICODE
        PUBLIC _UNICODE
    )
endif ()

target_include_directories (${libname}-impl
    PUBLIC ${PROJECT_BINARY_DIR}
    PUBLIC ${PROJECT_SOURCE_DIR}
    PUBLIC ${Poco_INCLUDE_DIRS}
)

if(USE_SSL)
    target_link_libraries(${libname}-impl
        PUBLIC ${Poco_NetSSL_LIBRARY}
        PUBLIC ${Poco_Crypto_LIBRARY}
    )
    target_include_directories (${libname}-impl
        PUBLIC ${OPENSSL_INCLUDE_DIR}
    )
endif()

target_link_libraries(${libname}-impl
    PUBLIC ${Poco_Net_LIBRARY}
    PUBLIC ${Poco_Foundation_LIBRARY}
    PUBLIC ${Poco_Util_LIBRARY}
    PUBLIC ODBC::Driver
    PUBLIC Threads::Threads
)

add_library(${libname} SHARED
    api/odbc.cpp
    api/impl/handles.cpp
    api/impl/attr.cpp
    api/impl/info.cpp

    ${WIN_SOURCES}
)

set_target_properties(${libname} PROPERTIES
    OUTPUT_NAME "clickhouseodbc${WIDECHAR_SUFFIX}"
)

target_link_libraries(${libname}
    PRIVATE ${libname}-impl
)

if (MSVC)
    install(
        TARGETS ${libname}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT "${DRIVER_PREFIX}Driver"
    )
    install(
        FILES $<TARGET_PDB_FILE:${libname}>
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT "${DRIVER_PREFIX}DriverDebugSymbols"
        OPTIONAL
    )
else()
    install(
        TARGETS ${libname}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT "${DRIVER_PREFIX}Driver"
    )
endif()

endfunction()

declare_odbc_lib_targets(clickhouse-odbc 0)
declare_odbc_lib_targets(clickhouse-odbcw 1)

if(CLICKHOUSE_ODBC_TEST)
    add_subdirectory(test)
endif()
