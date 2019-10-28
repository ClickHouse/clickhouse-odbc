#include "gtest_env.h"

#include <stdexcept>

TestEnvironment * TestEnvironment::environment_ = nullptr;

TestEnvironment::TestEnvironment::TestEnvironment(int argc, char * argv[]) {
    for (std::size_t i = 1; i < argc; ++i) {
        command_line_params_.emplace_back(argv[i]);
    }

    if (environment_ == nullptr)
        environment_ = this;
}

TestEnvironment::~TestEnvironment() {
    if (environment_ == this)
        environment_ = nullptr;
}

TestEnvironment & TestEnvironment::getInstance() {
    if (environment_ == nullptr)
        throw std::runtime_error("TestEnvironment instance not available");

    return *environment_;
}

const std::string& TestEnvironment::getDSN() {
    if (command_line_params_.size() != 1)
        throw std::runtime_error("Unable to extract positional command-line parameter value for DSN");

    return command_line_params_[0];
};
