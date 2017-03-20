add_library(clickhouse-odbc SHARED
    attr.cpp
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
    PocoNet
    PocoFoundation
)
