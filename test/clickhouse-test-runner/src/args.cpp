#include "args.h"

#include <cstdlib>
#include <regex>
#include <stdexcept>

namespace runner {

namespace {

    // The command-line settings that the shim sends to the server. `tests/clickhouse-test` also adds
    // many randomized settings to the command line. The shim drops them: they only change
    // performance-related paths in the server, and some of them are client-only.
    bool isServerSetting(const std::string & name) {
        return name == "log_comment" || name == "max_insert_threads" || name == "max_threads" || name == "send_logs_level";
    }

    struct OptionSpec {
        const char * long_name;
        const char * short_name;
        bool has_arg;
    };

    const OptionSpec known_options[] = {
        {"host", "h", true},
        {"port", nullptr, true},
        {"user", "u", true},
        {"password", nullptr, true},
        {"database", "d", true},
        {"query", "q", true},
        {"secure", "s", false},
        {"no-secure", nullptr, false},
        // As in clickhouse-client: `-n` is `--multiquery`, `-m` is `--multiline`. The shim always
        // runs every statement of the input.
        {"multiquery", "n", false},
        {"multi-query", nullptr, false},
        {"multiline", "m", false},
        {"help", nullptr, false},
        // Accepted and ignored.
        {"config-file", "C", true},
        {"client_logs_file", nullptr, true},
        {"allow_repeated_settings", nullptr, false},
        {"fake-drop", nullptr, false},
    };

    const OptionSpec * findOption(const std::string & long_name, const std::string & short_name) {
        for (const auto & o : known_options) {
            if ((!long_name.empty() && long_name == o.long_name) || (!short_name.empty() && o.short_name && short_name == o.short_name))
                return &o;
        }
        return nullptr;
    }

    bool isNegativeNumber(const std::string & s) {
        static const std::regex re(R"(^-(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$)");
        return std::regex_match(s, re);
    }

    void set(Options & opts, const std::string & name, const std::string & value) {
        if (name == "host") {
            opts.host = value;
        } else if (name == "port") {
            char * end = nullptr;
            const long p = std::strtol(value.c_str(), &end, 10);
            if (value.empty() || *end != '\0' || p < 0 || p > 65535)
                throw std::invalid_argument("bad --port value \"" + value + "\"");
            opts.port = static_cast<int>(p);
        } else if (name == "user") {
            opts.user = value;
        } else if (name == "password") {
            opts.password = value;
        } else if (name == "database") {
            opts.database = value;
        } else if (name == "query") {
            opts.query = value;
        } else if (name == "secure") {
            opts.secure = true;
        } else if (name == "no-secure") {
            opts.secure = false;
        } else if (name == "help") {
            opts.help = true;
        }
    }

    void addSetting(Options & opts, const std::string & name, const std::string & value) {
        for (auto & [n, v] : opts.settings) {
            if (n == name) {
                v = value;
                return;
            }
        }
        opts.settings.emplace_back(name, value);
    }

} // namespace

Options parseArgs(const std::vector<std::string> & argv) {
    Options opts;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        const auto & arg = argv[i];
        if (arg == "--")
            break;
        if (arg.starts_with("--")) {
            const auto eq = arg.find('=');
            const auto name = arg.substr(2, eq == std::string::npos ? std::string::npos : eq - 2);
            bool has_value = (eq != std::string::npos);
            std::string value = has_value ? arg.substr(eq + 1) : std::string();
            if (name.empty())
                continue;
            if (const auto * o = findOption(name, "")) {
                if (!o->has_arg) {
                    set(opts, o->long_name, "");
                    continue;
                }
                if (!has_value) {
                    if (i + 1 >= argv.size())
                        throw std::invalid_argument("option --" + name + " needs a value");
                    value = argv[++i];
                }
                set(opts, o->long_name, value);
                continue;
            }
            // Any other long option is a setting (`--max_threads=1`) or a query parameter
            // (`--param_x=1`). The value is optional.
            if (!has_value && i + 1 < argv.size()) {
                const auto & next = argv[i + 1];
                if (!next.starts_with("-") || isNegativeNumber(next)) {
                    value = next;
                    has_value = true;
                    ++i;
                }
            }
            if (!has_value)
                value = "1";
            if (name.starts_with("param_")) {
                opts.params[name.substr(6)] = value;
                continue;
            }
            std::string setting = name;
            for (auto & c : setting) {
                if (c == '-')
                    c = '_';
            }
            if (isServerSetting(setting))
                addSetting(opts, setting, value);
        } else if (arg.starts_with("-") && arg.size() > 1) {
            const auto * o = findOption("", arg.substr(1));
            if (!o) {
                // Unknown short options are ignored, as the other language ports do.
                continue;
            }
            if (!o->has_arg) {
                set(opts, o->long_name, "");
                continue;
            }
            if (i + 1 >= argv.size())
                throw std::invalid_argument("option " + arg + " needs a value");
            set(opts, o->long_name, argv[++i]);
        }
        // Positional arguments (such as `client` in `clickhouse client`) are ignored.
    }
    return opts;
}

std::string redactedCommandLine(const std::vector<std::string> & argv) {
    // Reads the options as parseArgs does, so that the value of each option is known.
    std::string joined;
    const auto add = [&](const std::string & s) { joined += (joined.empty() ? "" : " ") + s; };
    const auto redact = [](const std::string & value) { return value.empty() ? value : std::string("<redacted>"); };
    for (std::size_t i = 0; i < argv.size(); ++i) {
        const auto & arg = argv[i];
        const OptionSpec * o = nullptr;
        if (arg == "--") {
            for (; i < argv.size(); ++i)
                add(argv[i]);
            break;
        } else if (arg.starts_with("--")) {
            const auto eq = arg.find('=');
            if (eq != std::string::npos) {
                const bool password = (arg.substr(2, eq - 2) == "password");
                add(password ? "--password=" + redact(arg.substr(eq + 1)) : arg);
                continue;
            }
            o = findOption(arg.substr(2), "");
        } else if (arg.starts_with("-") && arg.size() > 1) {
            o = findOption("", arg.substr(1));
        }
        add(arg);
        if (o && o->has_arg && i + 1 < argv.size()) {
            ++i;
            add(std::string_view(o->long_name) == "password" ? redact(argv[i]) : argv[i]);
        }
    }
    return joined;
}

namespace {

    int envInt(const char * name, int def) {
        const char * v = std::getenv(name);
        if (!v || !*v)
            return def;
        char * end = nullptr;
        const long n = std::strtol(v, &end, 10);
        return (*end == '\0') ? static_cast<int>(n) : def;
    }

} // namespace

int httpPort(int port, bool secure) {
    const char * native_env = secure ? "CLICKHOUSE_PORT_TCP_SECURE" : "CLICKHOUSE_PORT_TCP";
    const char * http_env = secure ? "CLICKHOUSE_PORT_HTTPS" : "CLICKHOUSE_PORT_HTTP";
    const int native = envInt(native_env, secure ? 9440 : 9000);
    if (port != 0 && port != native)
        return port;
    return envInt(http_env, secure ? 8443 : 8123);
}

const char * const usage_text = R"(usage: clickhouse-client [options]

A clickhouse-client imitation that runs queries through the ClickHouse ODBC driver, so that the
ClickHouse tests/clickhouse-test runner can drive it. If --query is not given, the queries are read
from stdin. Every ";"-separated statement runs.

Options:
  -h, --host HOST          Server host (default: localhost)
      --port PORT          Server port. The native TCP port of the test runner
                           (CLICKHOUSE_PORT_TCP) maps to CLICKHOUSE_PORT_HTTP.
  -u, --user USER          User name (default: default)
      --password PASSWORD  Password (default: empty)
  -d, --database DB        Database (default: default)
  -q, --query SQL          Queries to run
  -s, --secure             Use TLS (https)
      --param_NAME=VALUE   Query parameter
      --SETTING=VALUE      log_comment, max_insert_threads, max_threads and
                           send_logs_level go to the server; other settings
                           are ignored
  -n, --multiquery         Ignored
  -m, --multiline          Ignored
      --help               Show this help

Environment variables:
  CLICKHOUSE_TEST_RUNNER_DRIVER   Path of the driver library (libclickhouseodbc.so)
  CLICKHOUSE_TEST_RUNNER_BACKEND  odbcdriver2 (default) or rowbinary
  CLICKHOUSE_CLIENT_CLI_LOG       Log file for troubleshooting
)";

} // namespace runner
