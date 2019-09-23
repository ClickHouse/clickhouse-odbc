find_package (Threads)

add_subdirectory(escaping)

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

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config_cmake.h.in ${CMAKE_CURRENT_BINARY_DIR}/config_cmake${_config_add}.h)

# In order to enable testing, put every non-public symbol to a static library (which is then used by shared library and unit-test binary).
add_library(${libname}_static STATIC
    attributes.cpp
    config.cpp
    connection.cpp
    descriptor.cpp
    diagnostics.cpp
    driver.cpp
    environment.cpp
    object.cpp
    read_helpers.cpp
    result_set.cpp
    statement.cpp
    type_info.cpp
    type_parser.cpp

    attributes.h
    config.h
    connection.h
    descriptor.h
    diagnostics.h
    driver.h
    environment.h
    ini_defines.h
    iostream_debug_helpers.h
    object.h
    platform.h
    read_helpers.h
    result_set.h
    scope_guard.h
    statement.h
    string_ref.h
    type_info.h
    type_parser.h
    unicode_t.h
    utils.h
)

set (WIN_SOURCES)
if (WIN32)
    set (WIN_SOURCES
        "win/dllmain.cpp"
        "win/resource.h"
        "win/setup.cpp"
        "win/version.h"
        "win/resource.rc"
        "win/${libname}.def"
    )
endif()

add_library(${libname} SHARED
    odbc.cpp
    handles.cpp
    attr.cpp
    info.cpp

    ${WIN_SOURCES}
)

if (UNICODE)
    target_compile_definitions(${libname}_static
        PUBLIC UNICODE=1
        PUBLIC _UNICODE=1
    )

    # Mode for unixodbc for working with wchar as in iodbc
    if (SQL_WCHART_CONVERT)
        target_compile_definitions(${libname}_static PUBLIC SQL_WCHART_CONVERT=1)
    endif ()

    #if(ODBC_IODBC)
    #    target_compile_definitions(${libname}_static PUBLIC SQL_NOUNICODEMAP=1)
    #endif()
endif ()

target_compile_definitions (${libname}_static PUBLIC CMAKE_BUILD=1)

target_include_directories (${libname}_static PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories (${libname}_static PUBLIC ${Poco_INCLUDE_DIRS})
target_include_directories (${libname}_static PUBLIC ${ODBC_INCLUDE_DIRECTORIES})

target_link_libraries(${libname}_static PUBLIC clickhouse-odbc-escaping)

if(USE_SSL)
    target_link_libraries(${libname}_static PUBLIC ${Poco_NetSSL_LIBRARY} ${Poco_Crypto_LIBRARY})
    target_include_directories (${libname}_static PUBLIC ${OPENSSL_INCLUDE_DIR})
endif()

target_link_libraries(${libname}_static
    PUBLIC ${Poco_Net_LIBRARY}
    PUBLIC ${Poco_Foundation_LIBRARY}
    PUBLIC ${Poco_Util_LIBRARY}
    PUBLIC ${ODBC_LIBRARIES}
)

if(NOT APPLE AND NOT WIN32)
    target_link_libraries(${libname}_static PUBLIC "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/linker_script")
endif()

if(WIN32)
    target_link_libraries(${libname}_static PUBLIC ws2_32 Iphlpapi legacy_stdio_definitions)
endif()

if(ARCH_LINUX)
    target_link_libraries(${libname}_static PUBLIC Threads::Threads)
endif()

target_link_libraries(${libname} PRIVATE ${libname}_static)

if(NOT WIN32)
    install(TARGETS ${libname} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/odbc)
endif()

endfunction()

declare_odbc_lib_targets(clickhouse-odbc 0)
declare_odbc_lib_targets(clickhouse-odbcw 1)

set_target_properties(clickhouse-odbc PROPERTIES OUTPUT_NAME "clickhouseodbc")
set_target_properties(clickhouse-odbcw PROPERTIES OUTPUT_NAME "clickhouseodbcw")

if (NOT WIN32)
    install(FILES unixodbc.ini DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/clickhouse-odbc)
    install(FILES odbc.ini DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples)
endif()

if(CLICKHOUSE_ODBC_TEST)
    add_subdirectory(ut)
endif()
