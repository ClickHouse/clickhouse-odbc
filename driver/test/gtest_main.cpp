#include "driver/test/gtest_env.h"

#include <gtest/gtest.h>

int main(int argc, char * argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new TestEnvironment(argc, argv));
    return RUN_ALL_TESTS();
}
