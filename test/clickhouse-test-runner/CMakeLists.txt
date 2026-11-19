# The clickhouse-client imitation that runs the ClickHouse core SQL tests through the driver.
# A separate small project: it is an ODBC application, so it links the system driver manager
# (unixODBC), not the driver. The driver library is a run-time argument (Driver= of the
# connection string). See README.md.
#
#   cmake -S test/clickhouse-test-runner -B test/clickhouse-test-runner/.build -G Ninja
#   cmake --build test/clickhouse-test-runner/.build
#   test/clickhouse-test-runner/.build/clickhouse-test-runner-unit-tests

cmake_minimum_required(VERSION 3.16)

project(clickhouse-test-runner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(CH_ODBC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "The clickhouse-odbc checkout")
set(CLICKHOUSE_TEST_RUNNER_DEFAULT_DRIVER "${CH_ODBC_SOURCE_DIR}/build/driver/libclickhouseodbc.so"
    CACHE FILEPATH "The driver library when CLICKHOUSE_TEST_RUNNER_DRIVER is not set")

find_package(ODBC REQUIRED)

add_library(clickhouse-test-runner-lib STATIC
    src/args.cpp
    src/errorcodes.cpp
    src/format.cpp
    src/hint.cpp
    src/log.cpp
    src/odbc.cpp
    src/render.cpp
    src/runner.cpp
    src/split.cpp
    src/text.cpp
)
target_include_directories(clickhouse-test-runner-lib PUBLIC src)
target_link_libraries(clickhouse-test-runner-lib PUBLIC ODBC::ODBC)
target_compile_options(clickhouse-test-runner-lib PUBLIC -Wall -Wextra)

add_executable(clickhouse-test-runner src/main.cpp)
target_link_libraries(clickhouse-test-runner PRIVATE clickhouse-test-runner-lib)
target_compile_definitions(clickhouse-test-runner PRIVATE
    CLICKHOUSE_TEST_RUNNER_DEFAULT_DRIVER="${CLICKHOUSE_TEST_RUNNER_DEFAULT_DRIVER}")

option(CLICKHOUSE_TEST_RUNNER_UNIT_TESTS "Build the unit tests (googletest from contrib/)" ON)
if(CLICKHOUSE_TEST_RUNNER_UNIT_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    # The googletest of contrib/ asks for CMake 2.8.12 policies, which CMake 4 removed. The driver's
    # workflows give -DCMAKE_POLICY_VERSION_MINIMUM=3.5 for the same reason.
    if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    endif()
    add_subdirectory("${CH_ODBC_SOURCE_DIR}/contrib/googletest" googletest EXCLUDE_FROM_ALL)

    add_executable(clickhouse-test-runner-unit-tests
        tests/args_test.cpp
        tests/format_test.cpp
        tests/hint_test.cpp
        tests/render_test.cpp
        tests/runner_test.cpp
        tests/split_test.cpp
    )
    target_link_libraries(clickhouse-test-runner-unit-tests PRIVATE clickhouse-test-runner-lib GTest::gtest_main)

    enable_testing()
    add_test(NAME clickhouse-test-runner-unit-tests COMMAND clickhouse-test-runner-unit-tests)
endif()
