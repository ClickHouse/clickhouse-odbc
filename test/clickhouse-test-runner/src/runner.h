#pragma once

#include "args.h"
#include "format.h"
#include "log.h"
#include "render.h"
#include "split.h"

#include <sql.h>
#include <sqlext.h>

#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace runner {

// The result format that the driver reads from the server (the `default_format` of the URL).
enum class Backend {
    ODBCDriver2, // the driver's default
    RowBinary,   // RowBinaryWithNamesAndTypes, documented as experimental
};

// A result that does not match the test hint.
class HintError : public std::runtime_error {
public:
    HintError(const std::string & message, int code_) : std::runtime_error(message), code(code_) { }
    int code;
};

// Runs the statements of one shim invocation in one session.
//
// The session: all requests of the invocation carry the same `session_id`, which the shim gives in
// the query string of the driver's `Url` (the driver sends the URL parameters with each request,
// and keeps a session_id of the URL). `SET`, `USE` and temporary tables last, also after an error.
// The driver's AutoSessionId option is not used: it makes a new session_id for each connection,
// and the shim must connect again in one case (below).
//
// The command-line settings and query parameters go in the query string of the `Url` too. As in
// clickhouse-client, a `SET` of the same setting replaces the command-line value: the shim then
// connects again, with the same session_id and a URL without the setting. After a `USE`, the shim
// sets the current catalog (SQL_ATTR_CURRENT_CATALOG), as clickhouse-client sets its current
// database: the driver sends the database of the connection with each request, and that database
// replaces the current database of the session.
class Runner {
public:
    Runner(Options opts, Backend backend, std::string driver_path, InvocationLog & log, std::FILE * out);
    ~Runner();
    Runner(const Runner &) = delete;
    Runner & operator=(const Runner &) = delete;

    // Runs the statements in order, and stops at the first statement whose result does not match
    // its test hint, like clickhouse-client. Throws OdbcError, HintError or std::runtime_error.
    void run(const std::vector<Statement> & statements);

    // With redact_password, a password that is not empty is `<redacted>`: the text for the log.
    std::string connectionString(bool redact_password = false) const;
    std::string url() const;

private:
    void connect();
    void runOne(const Statement & st);
    void execute(const std::string & sql, bool expects_error, std::string & result);
    void writeRows(SQLHSTMT stmt, SQLSMALLINT column_count, const OutputFormat & format, std::string & result);
    void readValue(SQLHSTMT stmt, SQLUSMALLINT column, const ColumnInfo & info, bool raw, std::string & out);
    void afterSuccess(const std::string & sql);
    void echo(const Statement & st);
    bool otherDialect(const std::string & sql) const;

    Options opts;
    Backend backend;
    std::string driver_path;
    InvocationLog & log;
    std::FILE * out;

    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;
    bool connected = false;

    // The session_id of all the requests of the invocation.
    std::string session_id;
    // The current database of the session.
    std::string database;
    // The command-line settings that go with each query.
    std::vector<std::pair<std::string, std::string>> settings;
    // The value of the last `SET dialect`.
    std::string dialect;
};

// Parses the database name of a `USE` statement. A `{name:Identifier}` query parameter is replaced
// by its value.
bool parseUseDatabase(const std::string & sql, const std::map<std::string, std::string> & params, std::string & database);

// Returns the settings that a `SET a = 1, b = 'x'` statement assigns, with their values (string
// values without quotes).
std::vector<std::pair<std::string, std::string>> parseSetAssignments(const std::string & sql);

// Percent-encodes a URL query component.
std::string urlEncode(std::string_view s);

// Tells if sql is a `let` statement of the Kusto dialect. clickhouse-client parses a Kusto query
// with ParserKQLStatement, which reads the `let name = ...;` statements in front of a query as a
// part of it, so `let a = 1; T | where x > a;` is one query.
bool isKustoLet(std::string_view dialect, std::string_view sql);

// The number of `{` minus the number of `}` in sql, outside quotes and comments. A Kusto function
// body (`let f = (x:long) { let y = x + 1; y * 2 };`) can have a `;` inside the braces.
int kustoBraceDepth(std::string_view sql);

// Tells if a server error shows that the statement has a syntax error. clickhouse-client parses
// each statement before it sends it, and does not echo a statement that it cannot parse. The
// server sends SYNTAX_ERROR also for a text that the query parses at run time.
bool isStatementSyntaxError(std::optional<int> code, std::string_view server_message);

// One statement from a statement and the statement after it: the text of both, the hint of the
// second.
Statement joinStatements(const Statement & first, const Statement & second);

// A new random session_id.
std::string newSessionId();

} // namespace runner
