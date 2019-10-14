add_library(clickhouse-odbc-escaping
    escape_sequences.cpp
    lexer.cpp
)

target_include_directories(clickhouse-odbc-escaping PUBLIC ${PROJECT_BINARY_DIR}/driver)
target_include_directories(clickhouse-odbc-escaping PUBLIC ${PROJECT_SOURCE_DIR}/driver)
