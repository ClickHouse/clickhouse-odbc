function(declare_odbc_lib_targets libname UNICODE)

#option (UNICODE "Enable unicode" 0)
if (UNICODE)
    set(UNICODE 1)
    if (ODBC_UNIXODBC)
        option (SQL_WCHART_CONVERT "Unixodbc SQL_WCHART_CONVERT (use four bytes TCHAR)" 0)
    endif ()
    if(ODBC_IODBC)
        #option (ODBC_CHAR16 "Strange option for iodbc" 1)
    endif()
    if (ODBC_IODBC AND APPLE)
        set(ODBC_WCHAR 1)
    endif()

    set(_config_add w)
else ()
    set(UNICODE 0)
endif ()

# Hell matrix
#               TCHAR       sizeof()    note
# not unicode   char        1
# unicode:
# win           wchar_t     2
# iodbc LINUX   wchar_t     4           wchar_t != TCHAR
# iodbc APPLE   wchar_t     4           wchar_t == TCHAR
# unixodbc      wchar_t     4           true == SQL_WCHART_CONVERT
# unixodbc      char16_t    2           false == SQL_WCHART_CONVERT

message (STATUS "${libname}: UNICODE=${UNICODE} SQL_WCHART_CONVERT=${SQL_WCHART_CONVERT} ODBC_WCHAR=${ODBC_WCHAR} ODBC_CHAR16=${ODBC_CHAR16}")

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/platform/config_cmake.h.in ${CMAKE_CURRENT_BINARY_DIR}/platform/config_cmake${_config_add}.h)

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
    object.cpp
    result_set.cpp
    statement.cpp
    type_info.cpp


    platform/platform.h
    platform/unicode_t.h

    utils/utils.h
    utils/iostream_debug_helpers.h
    utils/read_helpers.h
    utils/type_parser.h
    utils/scope_guard.h
    utils/string_ref.h

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
        platform/win/${libname}.def
    )
endif()

add_library(${libname} SHARED
    api/odbc.cpp
    api/impl/handles.cpp
    api/impl/attr.cpp
    api/impl/info.cpp

    ${WIN_SOURCES}
)

if (UNICODE)
    target_compile_definitions(${libname}-impl
        PUBLIC UNICODE=1
        PUBLIC _UNICODE=1
    )

    # Mode for unixodbc for working with wchar as in iodbc
    if (SQL_WCHART_CONVERT)
        target_compile_definitions(${libname}-impl PUBLIC SQL_WCHART_CONVERT=1)
    endif ()

    #if(ODBC_IODBC)
    #    target_compile_definitions(${libname}-impl PUBLIC SQL_NOUNICODEMAP=1)
    #endif()
endif ()

target_include_directories (${libname}-impl
    PUBLIC ${PROJECT_BINARY_DIR}
    PUBLIC ${PROJECT_SOURCE_DIR}
    PUBLIC ${Poco_INCLUDE_DIRS}
    PUBLIC ${ODBC_INCLUDE_DIRECTORIES}
)

if(USE_SSL)
    target_link_libraries(${libname}-impl
        PUBLIC ${Poco_NetSSL_LIBRARY}
        PUBLIC ${Poco_Crypto_LIBRARY}
    )
    target_include_directories (${libname}-impl PUBLIC ${OPENSSL_INCLUDE_DIR})
endif()

target_link_libraries(${libname}-impl
    PUBLIC ${Poco_Net_LIBRARY}
    PUBLIC ${Poco_Foundation_LIBRARY}
    PUBLIC ${Poco_Util_LIBRARY}
    PUBLIC ${ODBC_LIBRARIES}
)

if(WIN32)
    target_link_libraries(${libname}-impl PUBLIC ws2_32 Iphlpapi legacy_stdio_definitions)
endif()

if(ARCH_LINUX)
    target_link_libraries(${libname}-impl PUBLIC Threads::Threads)
endif()

target_link_libraries(${libname} PRIVATE ${libname}-impl)

if (UNICODE)
    set(DRIVER_PREFIX Unicode)
else()
    set(DRIVER_PREFIX ANSI)
endif()

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

set_target_properties(clickhouse-odbc PROPERTIES OUTPUT_NAME "clickhouseodbc")
set_target_properties(clickhouse-odbcw PROPERTIES OUTPUT_NAME "clickhouseodbcw")

if(CLICKHOUSE_ODBC_TEST)
    add_subdirectory(test)
endif()
