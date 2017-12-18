add_library(clickhouse-odbc-escaping
    escape_sequences.cpp
    lexer.cpp
)

target_include_directories(clickhouse-odbc-escaping PUBLIC ..)
