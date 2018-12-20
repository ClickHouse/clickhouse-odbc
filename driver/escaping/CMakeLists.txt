add_library(clickhouse-odbc-escaping
    escape_sequences.cpp
    lexer.cpp
)

target_include_directories(clickhouse-odbc-escaping PUBLIC ..)
target_link_libraries(clickhouse-odbc-escaping PRIVATE clickhouse-odbc-log)
