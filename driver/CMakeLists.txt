add_subdirectory(escaping)

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
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(clickhouse-odbc
        PROPERTIES PREFIX ""
               OUTPUT_NAME "clickhouse-odbc"
    )
else()
    set_target_properties(clickhouse-odbc PROPERTIES OUTPUT_NAME "clickhouseodbc")
endif()

target_link_libraries(clickhouse-odbc
    clickhouse-odbc-escaping

    ${Poco_Net_LIBRARY}
    ${Poco_Foundation_LIBRARY}
    ${ODBC_LIBRARIES}
)

install(TARGETS clickhouse-odbc LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/odbc)
install(FILES unixodbc.ini DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/clickhouse-odbc)
install(FILES odbc.ini DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples)

if(CLICKHOUSE_ODBC_TEST)
    add_subdirectory(ut)
endif()
