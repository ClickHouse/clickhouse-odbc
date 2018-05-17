cd ..

md build
cd build
cmake .. -G "Visual Studio 15 2017 Win64" && cmake --build . -- /m
cd ..

md build32
cd build32
cmake .. -G "Visual Studio 15 2017" && cmake --build . -- /m
cd ..

md buildw
cd build
cmake .. -G "Visual Studio 15 2017 Win64" -DUNICODE=1 && cmake --build . -- /m
cd ..

md buildw32
cd buildw32
cmake .. -G "Visual Studio 15 2017" -DUNICODE=1 && cmake --build . -- /m
cd ..
