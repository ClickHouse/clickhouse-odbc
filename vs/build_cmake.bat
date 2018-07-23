cd ..

md build
cd build
cmake .. -G "Visual Studio 15 2017 Win64" -DTEST_DSN=clickhouse_localhost && cmake --build . -- /m
cd ..

md build32
cd build32
cmake .. -G "Visual Studio 15 2017" -DTEST_DSN=clickhouse_localhost && cmake --build . -- /m
cd ..

md buildw
cd buildw
cmake .. -G "Visual Studio 15 2017 Win64" -DUNICODE=1 -DTEST_DSN=clickhouse_localhost && cmake --build . -- /m
cd ..

md buildw32
cd buildw32
cmake .. -G "Visual Studio 15 2017" -DUNICODE=1 -DTEST_DSN=clickhouse_localhost && cmake --build . -- /m
cd ..
