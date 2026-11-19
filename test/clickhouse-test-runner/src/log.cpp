#include "log.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <fcntl.h>
#include <unistd.h>

namespace runner {

InvocationLog::InvocationLog() {
    if (const char * p = std::getenv("CLICKHOUSE_CLIENT_CLI_LOG"))
        path = p;
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    add("=== clickhouse-client invocation", buf);
}

InvocationLog::~InvocationLog() {
    if (path.empty())
        return;
    // A log that cannot be written must not change the result of a test, so errors are ignored
    // here; the missing lines in the log show them.
    const int fd = ::open(path.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0)
        return;
    const auto written = ::write(fd, lines.data(), lines.size());
    (void)written;
    ::close(fd);
}

void InvocationLog::add(std::string_view key, std::string_view value) {
    if (path.empty())
        return;
    lines += key;
    lines += '=';
    for (const char c : value) {
        if (c == '\n')
            lines += "\\n";
        else
            lines += c;
    }
    lines += '\n';
}

} // namespace runner
