include_directories (${Poco_INCLUDE_DIRS})

add_library(clickhouse-odbc SHARED
    attr.cpp
    config.cpp
    connection.cpp
    diagnostics.cpp
    handles.cpp
    info.cpp
    odbc.cpp
    result_set.cpp
    statement.cpp
)

set_target_properties(clickhouse-odbc
    PROPERTIES PREFIX ""
               OUTPUT_NAME "odbc"
)

target_link_libraries(clickhouse-odbc
    ${Poco_Net_LIBRARY}
    ${Poco_Foundation_LIBRARY}
    odbc
)
