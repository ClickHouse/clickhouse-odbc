
add_executable(clickhouse-odbc-ut
    main.cpp
    escape_sequences_ut.cpp
    lexer_ut.cpp
)

target_link_libraries(clickhouse-odbc-ut
    clickhouse-odbc-escaping
    gtest-lib
    pthread
)

add_test(NAME "clickhouse-odbc-ut" COMMAND "clickhouse-odbc-ut")
