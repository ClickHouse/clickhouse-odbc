#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace runner {

// The `clickhouse-client` command line, as far as `tests/clickhouse-test` and the tests use it.
struct Options {
    std::string host = "localhost";
    int port = 0; // 0 when not given
    std::string user = "default";
    std::string password;
    std::string database = "default";
    bool secure = false;
    std::optional<std::string> query;
    bool help = false;

    // The command-line settings that go to the server with each query, in the order given.
    std::vector<std::pair<std::string, std::string>> settings;
    // The query parameters of `--param_<name>=<value>`.
    std::map<std::string, std::string> params;
};

// Parses argv (without the program name). Throws std::invalid_argument on a bad command line.
Options parseArgs(const std::vector<std::string> & argv);

// Maps the port that `tests/clickhouse-test` gives (the native TCP port, CLICKHOUSE_PORT_TCP) to
// the HTTP port of the server (CLICKHOUSE_PORT_HTTP). Any other port is used as given.
int httpPort(int port, bool secure);

extern const char * const usage_text;

} // namespace runner
