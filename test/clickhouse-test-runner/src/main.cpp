// clickhouse-test-runner imitates `clickhouse-client` well enough for the ClickHouse core test
// runner (`tests/clickhouse-test`) to drive it, and runs every query through the ClickHouse ODBC
// driver with the ODBC API (unixODBC). It reads the values that the driver returns and writes them
// as `TabSeparated` text, so the upstream `.reference` files check the driver.
//
// See README.md for how to run the upstream SQL tests with it.

#include "args.h"
#include "hint.h"
#include "log.h"
#include "odbc.h"
#include "runner.h"
#include "split.h"
#include "text.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#ifndef CLICKHOUSE_TEST_RUNNER_DEFAULT_DRIVER
#    define CLICKHOUSE_TEST_RUNNER_DEFAULT_DRIVER "libclickhouseodbc.so"
#endif

namespace {

using namespace runner;

// Answers `clickhouse extract-from-config --key <key>`, which `tests/clickhouse-test` runs to read
// the server config. Only `listen_host` has an answer; for other keys the output is empty, as for
// a key that is not in the config.
int extractFromConfig(const std::vector<std::string> & argv) {
    std::string key;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (argv[i] == "--key" && i + 1 < argv.size())
            key = argv[++i];
        else if (argv[i].starts_with("--key="))
            key = argv[i].substr(6);
    }
    if (key == "listen_host")
        std::fputs("127.0.0.1\n", stdout);
    return 0;
}

// Writes the error to stderr and returns the exit code. As in clickhouse-client, the exit code is
// the error code modulo 256 when there is one.
int reportError(const std::exception & e) {
    int code = 0;
    if (const auto * h = dynamic_cast<const HintError *>(&e)) {
        std::fprintf(stderr, "Error: %s\n", h->what());
        code = h->code;
    } else if (const auto * o = dynamic_cast<const OdbcError *>(&e); o && o->serverCode()) {
        std::string message = o->serverMessage();
        if (const auto name = errorNameByCode(*o->serverCode()); name && message.find("(" + *name + ")") == std::string::npos)
            message += " (" + *name + ")";
        std::fprintf(stderr, "Received exception from server:\n%s\n", message.c_str());
        code = *o->serverCode();
    } else {
        std::fprintf(stderr, "Error: %s\n", e.what());
    }
    return (code % 256 != 0) ? code % 256 : 1;
}

Backend backendFromEnv() {
    const char * v = std::getenv("CLICKHOUSE_TEST_RUNNER_BACKEND");
    const std::string name = v ? v : "";
    if (name.empty() || name == "odbcdriver2")
        return Backend::ODBCDriver2;
    if (name == "rowbinary")
        return Backend::RowBinary;
    throw std::invalid_argument("unknown CLICKHOUSE_TEST_RUNNER_BACKEND \"" + name + "\": use odbcdriver2 or rowbinary");
}

int run(const std::vector<std::string> & argv) {
    if (!argv.empty() && argv[0] == "extract-from-config")
        return extractFromConfig({argv.begin() + 1, argv.end()});

    InvocationLog log;
    // CI uploads the log as an artifact of a failed job.
    log.add("argv", redactedCommandLine(argv));

    Options opts;
    Backend backend = Backend::ODBCDriver2;
    try {
        opts = parseArgs(argv);
        backend = backendFromEnv();
    } catch (const std::exception & e) {
        std::fprintf(stderr, "Error: %s\n\n%s", e.what(), usage_text);
        return 1;
    }
    if (opts.help) {
        std::fputs(usage_text, stdout);
        return 0;
    }

    std::string text;
    if (opts.query)
        text = *opts.query;
    else
        text.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    if (trim(text).empty()) {
        std::fputs("Error: no query. Use --query or write the queries to stdin.\n", stderr);
        return 1;
    }

    const auto statements = splitStatements(text);
    log.add("statements", std::to_string(statements.size()));
    if (statements.empty())
        return 0;

    const char * driver = std::getenv("CLICKHOUSE_TEST_RUNNER_DRIVER");
    try {
        Runner r(opts, backend, (driver && *driver) ? driver : CLICKHOUSE_TEST_RUNNER_DEFAULT_DRIVER, log, stdout);
        log.add("url", r.url());
        r.run(statements);
    } catch (const std::exception & e) {
        log.add("error", e.what());
        std::fflush(stdout);
        return reportError(e);
    }
    return 0;
}

} // namespace

int main(int argc, char ** argv) {
    std::ios::sync_with_stdio(false);
    return run(std::vector<std::string>(argv + 1, argv + argc));
}
