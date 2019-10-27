#pragma once

#include <gtest/gtest.h>

#include <string>
#include <vector>

class TestEnvironment
    : public ::testing::Environment
{
public:
    TestEnvironment(int argc, char * argv[]);
    virtual ~TestEnvironment();
    
    static TestEnvironment & getInstance();

    const std::string& getDSN();

private:
    static TestEnvironment * environment_;
    
    std::vector<std::string> command_line_params_;
};
