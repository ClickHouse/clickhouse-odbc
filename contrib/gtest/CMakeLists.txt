add_library(gtest-lib STATIC
    gtest-all.cc
)

target_include_directories(gtest-lib PUBLIC ..) 
