add_subdirectory(escaping)
if (WIN32)
    add_subdirectory(win)
    #TODO: set(WIN_SOURCES "win/export.def")
endif()

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config_cmake.h.in ${CMAKE_CURRENT_BINARY_DIR}/config_cmake.h)

add_library(clickhouse-odbc SHARED
    attr.cpp
    config.cpp
    connection.cpp
    diagnostics.cpp
    environment.cpp
    handles.cpp
    info.cpp
    odbc.cpp
    result_set.cpp
    statement.cpp
    type_parser.cpp
    log.cpp
    ${WIN_SOURCES}
)


if (UNICODE AND NOT LIB_NAME_NO_W)
    set_target_properties(clickhouse-odbc PROPERTIES OUTPUT_NAME "clickhouseodbcw")
else()
    set_target_properties(clickhouse-odbc PROPERTIES OUTPUT_NAME "clickhouseodbc")
endif()

target_compile_definitions (clickhouse-odbc PRIVATE "-DCMAKE_BUILD=1")

target_include_directories (clickhouse-odbc PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if (NOT USE_INTERNAL_POCO_LIBRARY)
    target_include_directories (clickhouse-odbc PRIVATE ${Poco_INCLUDE_DIRS})
endif()

if (NOT WIN32)
    target_include_directories (clickhouse-odbc PRIVATE ${ODBC_INCLUDE_DIRECTORIES})
endif()

target_link_libraries(clickhouse-odbc clickhouse-odbc-escaping)

if(USE_SSL)
    target_link_libraries(clickhouse-odbc ${Poco_NetSSL_LIBRARY} ${Poco_Crypto_LIBRARY} ${Poco_Util_LIBRARY})
    target_include_directories (clickhouse-odbc PRIVATE ${OPENSSL_INCLUDE_DIR})
endif()

target_link_libraries(clickhouse-odbc
    ${Poco_Net_LIBRARY}
    ${Poco_Foundation_LIBRARY}
    ${ODBC_LIBRARIES}
)

if(NOT APPLE AND NOT WIN32)
    target_link_libraries(clickhouse-odbc "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/linker_script")
endif()

if (WIN32)
  target_link_libraries(clickhouse-odbc clickhouse-odbc-win ws2_32 Iphlpapi legacy_stdio_definitions)
endif()

if (NOT WIN32)
    install(TARGETS clickhouse-odbc LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/odbc)
    install(FILES unixodbc.ini DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/clickhouse-odbc)
    install(FILES odbc.ini DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples)
endif()

if(CLICKHOUSE_ODBC_TEST)
    add_subdirectory(ut)
endif()
