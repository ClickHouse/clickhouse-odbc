#pragma once

#include <string>
#include <string_view>

namespace runner {

// Collects troubleshooting lines for one invocation and appends them to the file named by
// CLICKHOUSE_CLIENT_CLI_LOG. Without that variable it does nothing. The lines are written in one
// write, so that the lines of parallel invocations do not mix.
class InvocationLog {
public:
    InvocationLog();
    ~InvocationLog();
    InvocationLog(const InvocationLog &) = delete;
    InvocationLog & operator=(const InvocationLog &) = delete;

    void add(std::string_view key, std::string_view value);

private:
    std::string path;
    std::string lines;
};

} // namespace runner
