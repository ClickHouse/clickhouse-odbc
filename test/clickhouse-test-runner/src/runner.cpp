#include "runner.h"
#include "odbc.h"
#include "text.h"

#include <cstring>
#include <memory>
#include <optional>
#include <random>

namespace runner {

namespace {

    constexpr int syntax_error_code = 62;

    // A statement handle that is freed at the end of the scope.
    class StatementHandle {
    public:
        explicit StatementHandle(SQLHDBC dbc) {
            check(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle), SQL_HANDLE_DBC, dbc, "SQLAllocHandle(SQL_HANDLE_STMT)");
        }
        ~StatementHandle() {
            if (handle != SQL_NULL_HSTMT)
                SQLFreeHandle(SQL_HANDLE_STMT, handle);
        }
        StatementHandle(const StatementHandle &) = delete;
        StatementHandle & operator=(const StatementHandle &) = delete;
        SQLHSTMT get() const {
            return handle;
        }

    private:
        SQLHSTMT handle = SQL_NULL_HSTMT;
    };

    // A value of a connection string, in braces when it has characters that end a value.
    std::string connectionStringValue(const std::string & v) {
        if (v.find_first_of(";{}") == std::string::npos && (v.empty() || (v.front() != ' ' && v.back() != ' ')))
            return v;
        std::string out = "{";
        for (const char c : v) {
            out += c;
            if (c == '}')
                out += '}';
        }
        return out + "}";
    }

} // namespace

std::string urlEncode(std::string_view s) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (isWordByte(ch) || c == '-' || c == '.' || c == '~') {
            out += ch;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

Runner::Runner(Options opts_, Backend backend_, std::string driver_path_, InvocationLog & log_, std::FILE * out_)
    : opts(std::move(opts_))
    , backend(backend_)
    , driver_path(std::move(driver_path_))
    , log(log_)
    , out(out_)
    , session_id(newSessionId())
    , database(opts.database)
    , settings(opts.settings) { }

Runner::~Runner() {
    if (connected)
        SQLDisconnect(dbc);
    if (dbc != SQL_NULL_HDBC)
        SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    if (env != SQL_NULL_HENV)
        SQLFreeHandle(SQL_HANDLE_ENV, env);
}

std::string Runner::url() const {
    const int port = httpPort(opts.port, opts.secure);
    std::string u = std::string(opts.secure ? "https" : "http") + "://" + opts.host + ":" + std::to_string(port) + "/";
    std::vector<std::string> query;
    if (!session_id.empty())
        query.push_back("session_id=" + urlEncode(session_id));
    if (backend == Backend::RowBinary)
        query.push_back("default_format=RowBinaryWithNamesAndTypes");
    for (const auto & [name, value] : settings)
        query.push_back(urlEncode(name) + "=" + urlEncode(value));
    for (const auto & [name, value] : opts.params)
        query.push_back("param_" + urlEncode(name) + "=" + urlEncode(value));
    for (std::size_t i = 0; i < query.size(); ++i)
        u += (i == 0 ? "?" : "&") + query[i];
    return u;
}

std::string Runner::connectionString(bool redact_password) const {
    // Driver= names the driver library that this repository builds; no DSN is needed.
    // Timeout: clickhouse-client waits 300 s for a response (receive_timeout); the driver's default
    // is 30 s, and some test queries take longer on the server.
    const auto password = (redact_password && !opts.password.empty()) ? std::string("<redacted>") : connectionStringValue(opts.password);
    return "Driver=" + connectionStringValue(driver_path) + ";Url=" + connectionStringValue(url())
        + ";UID=" + connectionStringValue(opts.user) + ";PWD=" + password + ";Database=" + connectionStringValue(database) + ";Timeout=300";
}

bool isKustoLet(std::string_view dialect, std::string_view sql) {
    if (!equalsIgnoreCase(dialect, "kusto"))
        return false;
    // Kusto has `//` line comments too.
    auto i = skipWhitespaceAndComments(sql, 0);
    while (sql.substr(i).starts_with("//"))
        i = skipWhitespaceAndComments(sql, skipLineComment(sql, i));
    return leadingKeyword(sql.substr(i)) == "LET";
}

int kustoBraceDepth(std::string_view sql) {
    int depth = 0;
    for (std::size_t i = 0; i < sql.size();) {
        const char c = sql[i];
        if (c == '\'' || c == '"' || c == '`')
            i = skipQuoted(sql, i);
        else if (startsLineComment(sql, i) || sql.substr(i).starts_with("//"))
            i = skipLineComment(sql, i);
        else if (sql.substr(i).starts_with("/*"))
            i = skipBlockComment(sql, i);
        else {
            if (c == '{')
                ++depth;
            else if (c == '}')
                --depth;
            ++i;
        }
    }
    return depth;
}

bool isStatementSyntaxError(std::optional<int> code, std::string_view server_message) {
    // A SYNTAX_ERROR that the server raises while it runs a query (formatQuery of a string, for
    // example) names the scope of the query (`: In scope SELECT ...`): the statement itself parsed.
    return code && *code == syntax_error_code && server_message.find(": In scope ") == std::string_view::npos;
}

Statement joinStatements(const Statement & first, const Statement & second) {
    Statement s;
    s.sql = first.full + "\n" + second.sql;
    s.full = first.full + "\n" + second.full;
    s.hint = second.hint;
    return s;
}

std::string newSessionId() {
    std::random_device rd;
    static const char hex[] = "0123456789abcdef";
    std::string id = "clickhouse-test-runner-";
    for (int i = 0; i < 32; ++i)
        id += hex[rd() % 16];
    return id;
}

void Runner::connect() {
    if (env == SQL_NULL_HENV) {
        check(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env), SQL_HANDLE_ENV, env, "SQLAllocHandle(SQL_HANDLE_ENV)");
        check(SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0),
            SQL_HANDLE_ENV,
            env,
            "SQLSetEnvAttr(SQL_ATTR_ODBC_VERSION)");
        check(SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc), SQL_HANDLE_ENV, env, "SQLAllocHandle(SQL_HANDLE_DBC)");
    }
    if (connected) {
        SQLDisconnect(dbc);
        connected = false;
    }
    const auto cs = connectionString();
    // CI uploads the log as an artifact of a failed job.
    log.add("connect", connectionString(/* redact_password */ true));
    SQLCHAR out_cs[4096];
    SQLSMALLINT out_len = 0;
    check(SQLDriverConnect(dbc,
              nullptr,
              reinterpret_cast<SQLCHAR *>(const_cast<char *>(cs.c_str())),
              SQL_NTS,
              out_cs,
              sizeof(out_cs),
              &out_len,
              SQL_DRIVER_NOPROMPT),
        SQL_HANDLE_DBC,
        dbc,
        "SQLDriverConnect");
    connected = true;
}

void Runner::run(const std::vector<Statement> & statements) {
    connect();
    // The Kusto dialect has its own statement rules (see isKustoLet and kustoBraceDepth). The
    // statements are split by SQL rules first: join them here, where the dialect is known.
    std::optional<Statement> lets; // complete `let` statements, in front of the next query
    std::optional<Statement> part; // a Kusto statement whose braces are not closed yet
    for (const auto & st : statements) {
        Statement current = part ? joinStatements(*part, st) : st;
        part.reset();
        if (equalsIgnoreCase(dialect, "kusto") && kustoBraceDepth(current.sql) > 0) {
            part = std::move(current);
            continue;
        }
        if (isKustoLet(dialect, current.sql) && !current.hint.expectsError() && current.hint.parse_error.empty()) {
            lets = lets ? joinStatements(*lets, current) : std::move(current);
            continue;
        }
        runOne(lets ? joinStatements(*lets, current) : current);
        lets.reset();
    }
    if (lets && part)
        runOne(joinStatements(*lets, *part));
    else if (lets || part)
        runOne(lets ? *lets : *part);
}

void Runner::echo(const Statement & st) {
    if (st.hint.echo_this) {
        std::fwrite(st.full.data(), 1, st.full.size(), out);
        std::fputc('\n', out);
    }
}

void Runner::runOne(const Statement & st) {
    if (!st.hint.parse_error.empty()) {
        echo(st);
        std::fflush(out);
        throw HintError(st.hint.parse_error, 0);
    }

    log.add("query", st.sql);
    std::string result;
    std::unique_ptr<OdbcError> error;
    try {
        execute(st.sql, st.hint.expectsError(), result);
    } catch (const OdbcError & e) {
        error = std::make_unique<OdbcError>(e);
    }
    const auto code = error ? error->serverCode() : std::nullopt;

    // clickhouse-client parses a statement before it echoes it, so it does not echo a statement
    // with a syntax error.
    if (!isStatementSyntaxError(code, error ? error->serverMessage() : std::string {}))
        echo(st);
    std::fwrite(result.data(), 1, result.size(), out);
    if (std::fflush(out) != 0)
        throw std::runtime_error("write stdout: " + std::string(std::strerror(errno)));

    if (st.hint.expectsError()) {
        if (!error)
            throw HintError("expected " + st.hint.label() + ", but the query succeeded (query: " + st.full + ")", 0);
        if (code && st.hint.matches(*code)) {
            log.add("expected_error_matched", st.hint.label());
            return;
        }
        if (code)
            throw HintError("expected " + st.hint.label() + ", but got " + error->serverMessage() + " (query: " + st.full + ")", *code);
    }
    if (error)
        throw *error;
    afterSuccess(st.sql);
}

bool Runner::otherDialect(const std::string & sql) const {
    return !dialect.empty() && !equalsIgnoreCase(dialect, "clickhouse") && leadingKeyword(sql) != "SET";
}

void Runner::execute(const std::string & statement, bool expects_error, std::string & result) {
    std::string sql = statement;
    const bool other = otherDialect(sql);
    if (other) {
        // Over HTTP the server parses the request body as one query, and the parsers of other
        // dialects do not accept a trailing semicolon.
        sql = trimRight(sql);
        if (!sql.empty() && sql.back() == ';')
            sql.pop_back();
    }
    OutputFormat format;
    std::string query = sql;
    if (!other && returnsRows(sql)) {
        // The FORMAT clause is SQL; other dialects can use the word differently.
        format = splitOutputFormat(sql, query);
    }
    if (!format.supported)
        throw std::runtime_error("output format " + format.name
            + " is not supported: the driver reads only its own formats, and the shim writes results only in the "
              "TabSeparated family");
    if (format.discard && !expects_error) {
        // `FORMAT Null`: send the statement unchanged, so that the server logs the query text of
        // the test (tests read it from system.query_log), and decides about the format: with
        // ignore_format_null_for_explain (on by default) it ignores `FORMAT Null` for EXPLAIN and
        // sends the plan in the driver's format. Otherwise the driver fails on the `Null` format.
        // A statement that expects an error keeps the clause removed: clickhouse-client applies
        // `FORMAT Null` itself, after the server sent all blocks, so errors of the output stage
        // occur only when the server writes the result (the `Null` format of HTTP writes nothing).
        query = sql;
        format = OutputFormat {};
    }
    if (query != sql)
        log.add("sent", query);

    StatementHandle stmt(dbc);
    const SQLRETURN rc
        = SQLExecDirect(stmt.get(), reinterpret_cast<SQLCHAR *>(const_cast<char *>(query.c_str())), static_cast<SQLINTEGER>(query.size()));
    if (rc == SQL_NO_DATA)
        return;
    try {
        check(rc, SQL_HANDLE_STMT, stmt.get(), "SQLExecDirect");
    } catch (const OdbcError & e) {
        if (!isNullFormatResult(e.sqlState(), e.message()))
            throw;
        log.add("null_format", "the server ran the statement; no rows");
        return;
    }

    SQLSMALLINT column_count = 0;
    check(SQLNumResultCols(stmt.get(), &column_count), SQL_HANDLE_STMT, stmt.get(), "SQLNumResultCols");
    if (column_count > 0)
        writeRows(stmt.get(), column_count, format, result);
}

void Runner::writeRows(SQLHSTMT stmt, SQLSMALLINT column_count, const OutputFormat & format, std::string & result) {
    std::vector<ColumnInfo> columns(column_count);
    SQLHDESC ard = SQL_NULL_HDESC;
    for (SQLUSMALLINT i = 1; i <= static_cast<SQLUSMALLINT>(column_count); ++i) {
        auto & c = columns[i - 1];
        SQLSMALLINT name_length = 0;
        std::vector<SQLCHAR> name(256);
        check(SQLDescribeCol(stmt,
                  i,
                  name.data(),
                  static_cast<SQLSMALLINT>(name.size()),
                  &name_length,
                  &c.sql_type,
                  &c.column_size,
                  &c.decimal_digits,
                  &c.nullable),
            SQL_HANDLE_STMT,
            stmt,
            "SQLDescribeCol");
        if (name_length >= static_cast<SQLSMALLINT>(name.size())) {
            name.resize(static_cast<std::size_t>(name_length) + 1);
            check(SQLDescribeCol(stmt,
                      i,
                      name.data(),
                      static_cast<SQLSMALLINT>(name.size()),
                      &name_length,
                      &c.sql_type,
                      &c.column_size,
                      &c.decimal_digits,
                      &c.nullable),
                SQL_HANDLE_STMT,
                stmt,
                "SQLDescribeCol");
        }
        c.name.assign(reinterpret_cast<const char *>(name.data()), name_length);

        SQLCHAR type_name[512] = {};
        SQLSMALLINT type_name_length = 0;
        check(SQLColAttribute(stmt, i, SQL_DESC_TYPE_NAME, type_name, sizeof(type_name), &type_name_length, nullptr),
            SQL_HANDLE_STMT,
            stmt,
            "SQLColAttribute(SQL_DESC_TYPE_NAME)");
        c.type_name = reinterpret_cast<const char *>(type_name);
        SQLLEN number = 0;
        check(SQLColAttribute(stmt, i, SQL_DESC_UNSIGNED, nullptr, 0, nullptr, &number),
            SQL_HANDLE_STMT,
            stmt,
            "SQLColAttribute(SQL_DESC_UNSIGNED)");
        c.is_unsigned = (number == SQL_TRUE);
        check(SQLColAttribute(stmt, i, SQL_DESC_PRECISION, nullptr, 0, nullptr, &c.precision),
            SQL_HANDLE_STMT,
            stmt,
            "SQLColAttribute(SQL_DESC_PRECISION)");
        check(SQLColAttribute(stmt, i, SQL_DESC_SCALE, nullptr, 0, nullptr, &c.scale),
            SQL_HANDLE_STMT,
            stmt,
            "SQLColAttribute(SQL_DESC_SCALE)");
        classifyColumn(c);

        if (c.kind == ColumnKind::Numeric) {
            // SQLGetData with SQL_C_NUMERIC uses the precision and scale of the ARD record: set the
            // ones of the column, and read with SQL_ARD_TYPE.
            if (ard == SQL_NULL_HDESC)
                check(SQLGetStmtAttr(stmt, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr),
                    SQL_HANDLE_STMT,
                    stmt,
                    "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)");
            check(SQLSetDescField(ard, i, SQL_DESC_TYPE, reinterpret_cast<SQLPOINTER>(SQL_C_NUMERIC), 0),
                SQL_HANDLE_DESC,
                ard,
                "SQLSetDescField(SQL_DESC_TYPE)");
            check(SQLSetDescField(ard, i, SQL_DESC_PRECISION, reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(c.precision)), 0),
                SQL_HANDLE_DESC,
                ard,
                "SQLSetDescField(SQL_DESC_PRECISION)");
            check(SQLSetDescField(ard, i, SQL_DESC_SCALE, reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(c.scale)), 0),
                SQL_HANDLE_DESC,
                ard,
                "SQLSetDescField(SQL_DESC_SCALE)");
        }
    }

    // A Raw format writes the header without escapes, as its rows (TabSeparatedRowOutputFormat::writeLine).
    auto header = [&](auto get) {
        for (std::size_t i = 0; i < columns.size(); ++i) {
            if (i > 0)
                result += '\t';
            if (format.raw)
                result += get(columns[i]);
            else
                writeEscaped(result, get(columns[i]));
        }
        result += '\n';
    };
    if (format.names)
        header([](const ColumnInfo & c) { return c.name; });
    if (format.types)
        header([](const ColumnInfo & c) { return c.type_name; });

    std::string row;
    while (true) {
        const SQLRETURN rc = SQLFetch(stmt);
        if (rc == SQL_NO_DATA)
            break;
        check(rc, SQL_HANDLE_STMT, stmt, "SQLFetch");
        row.clear();
        for (SQLUSMALLINT i = 1; i <= static_cast<SQLUSMALLINT>(column_count); ++i) {
            if (i > 1)
                row += '\t';
            readValue(stmt, i, columns[i - 1], format.raw, row);
        }
        row += '\n';
        // `FORMAT Null`: the values are read, but not written.
        if (!format.discard)
            result += row;
    }
}

void Runner::readValue(SQLHSTMT stmt, SQLUSMALLINT column, const ColumnInfo & c, bool raw, std::string & out_text) {
    SQLLEN indicator = 0;
    auto get = [&](SQLSMALLINT c_type, SQLPOINTER target, SQLLEN size) {
        const SQLRETURN rc = SQLGetData(stmt, column, c_type, target, size, &indicator);
        check(rc, SQL_HANDLE_STMT, stmt, "SQLGetData(column " + std::to_string(column) + " \"" + c.name + "\")");
        if (rc == SQL_SUCCESS_WITH_INFO) {
            std::string state;
            log.add("getdata_info", readDiagnostics(SQL_HANDLE_STMT, stmt, state));
        }
        return rc;
    };
    auto null = [&]() {
        if (indicator == SQL_NULL_DATA) {
            out_text += "\\N";
            return true;
        }
        return false;
    };

    switch (c.kind) {
        case ColumnKind::Bool: {
            unsigned char v = 0;
            get(SQL_C_BIT, &v, sizeof(v));
            if (!null())
                out_text += (v ? "true" : "false");
            return;
        }
        case ColumnKind::Signed:
        case ColumnKind::Unsigned: {
            union {
                signed char i8;
                unsigned char u8;
                short i16;
                unsigned short u16;
                SQLINTEGER i32;
                SQLUINTEGER u32;
                SQLBIGINT i64;
                SQLUBIGINT u64;
            } v {};
            get(c.c_type, &v, sizeof(v));
            if (null())
                return;
            switch (c.c_type) {
                case SQL_C_STINYINT:
                    out_text += std::to_string(v.i8);
                    break;
                case SQL_C_UTINYINT:
                    out_text += std::to_string(v.u8);
                    break;
                case SQL_C_SSHORT:
                    out_text += std::to_string(v.i16);
                    break;
                case SQL_C_USHORT:
                    out_text += std::to_string(v.u16);
                    break;
                case SQL_C_SLONG:
                    out_text += std::to_string(v.i32);
                    break;
                case SQL_C_ULONG:
                    out_text += std::to_string(v.u32);
                    break;
                case SQL_C_SBIGINT:
                    out_text += std::to_string(v.i64);
                    break;
                default:
                    out_text += std::to_string(v.u64);
                    break;
            }
            return;
        }
        case ColumnKind::Float32: {
            float v = 0;
            get(SQL_C_FLOAT, &v, sizeof(v));
            if (!null())
                out_text += formatFloat(v, 32);
            return;
        }
        case ColumnKind::Float64: {
            double v = 0;
            get(SQL_C_DOUBLE, &v, sizeof(v));
            if (!null())
                out_text += formatFloat(v, 64);
            return;
        }
        case ColumnKind::Numeric: {
            SQL_NUMERIC_STRUCT v {};
            get(SQL_ARD_TYPE, &v, sizeof(v));
            if (!null())
                out_text += formatNumeric(v);
            return;
        }
        case ColumnKind::Date: {
            SQL_DATE_STRUCT v {};
            get(SQL_C_TYPE_DATE, &v, sizeof(v));
            if (!null())
                out_text += formatDate(v);
            return;
        }
        case ColumnKind::Time: {
            SQL_TIME_STRUCT v {};
            get(SQL_C_TYPE_TIME, &v, sizeof(v));
            if (!null())
                out_text += formatTime(v);
            return;
        }
        case ColumnKind::Timestamp: {
            SQL_TIMESTAMP_STRUCT v {};
            get(SQL_C_TYPE_TIMESTAMP, &v, sizeof(v));
            if (!null())
                out_text += formatTimestamp(v, c.fraction_digits);
            return;
        }
        case ColumnKind::Guid: {
            SQLGUID v {};
            get(SQL_C_GUID, &v, sizeof(v));
            if (!null())
                out_text += formatGuid(v);
            return;
        }
        case ColumnKind::Text: {
            // Read the value in parts: SQLGetData returns SQL_SUCCESS_WITH_INFO (01004) while more
            // data is left. Each part ends with a terminating zero.
            std::string value;
            std::vector<char> buf(1 << 16);
            while (true) {
                const SQLRETURN rc = SQLGetData(stmt, column, SQL_C_CHAR, buf.data(), static_cast<SQLLEN>(buf.size()), &indicator);
                if (rc == SQL_NO_DATA)
                    break;
                check(rc, SQL_HANDLE_STMT, stmt, "SQLGetData(column " + std::to_string(column) + " \"" + c.name + "\")");
                if (indicator == SQL_NULL_DATA) {
                    out_text += "\\N";
                    return;
                }
                const std::size_t part = (indicator == SQL_NO_TOTAL || indicator >= static_cast<SQLLEN>(buf.size()))
                    ? buf.size() - 1
                    : static_cast<std::size_t>(indicator);
                value.append(buf.data(), part);
                if (rc == SQL_SUCCESS)
                    break;
            }
            if (raw)
                out_text += value;
            else
                writeEscaped(out_text, value);
            return;
        }
    }
}

void Runner::afterSuccess(const std::string & sql) {
    const auto keyword = leadingKeyword(sql);
    if (keyword == "USE") {
        std::string db;
        if (!parseUseDatabase(sql, opts.params, db))
            throw std::runtime_error("clickhouse-test-runner: cannot read the database name of \"" + sql + "\"");
        database = db;
        // The driver sends the database of the connection with each request; it replaces the
        // current database of the session. Set the one of the USE, as clickhouse-client does.
        check(SQLSetConnectAttr(
                  dbc, SQL_ATTR_CURRENT_CATALOG, const_cast<char *>(database.c_str()), static_cast<SQLINTEGER>(database.size())),
            SQL_HANDLE_DBC,
            dbc,
            "SQLSetConnectAttr(SQL_ATTR_CURRENT_CATALOG)");
        log.add("database", database);
    } else if (keyword == "SET" && !otherDialect(sql)) {
        bool reconnect = false;
        for (const auto & [name, value] : parseSetAssignments(sql)) {
            for (auto it = settings.begin(); it != settings.end(); ++it) {
                if (it->first == name) {
                    // The session holds the value now; a URL parameter would override it.
                    settings.erase(it);
                    reconnect = true;
                    break;
                }
            }
            if (name == "dialect")
                dialect = value;
        }
        if (reconnect)
            connect();
    }
}

bool parseUseDatabase(const std::string & sql, const std::map<std::string, std::string> & params, std::string & database) {
    auto i = skipWhitespaceAndComments(sql, 0) + 3;
    std::string rest = trim(std::string_view(sql).substr(std::min(i, sql.size())));
    if (!rest.empty() && rest.back() == ';')
        rest = trim(rest.substr(0, rest.size() - 1));
    if (const auto sp = rest.find(' '); sp != std::string::npos && equalsIgnoreCase(rest.substr(0, sp), "DATABASE"))
        rest = trim(rest.substr(sp + 1));
    if (rest.empty())
        return false;
    if (rest[0] == '`' || rest[0] == '"')
        return unquoteIdentifier(rest, database);
    if (rest[0] == '{') {
        const auto inner = trim(rest, "{}");
        const auto colon = inner.find(':');
        if (colon == std::string::npos)
            return false;
        const auto it = params.find(trim(inner.substr(0, colon)));
        if (it == params.end())
            return false;
        database = it->second;
        return true;
    }
    for (const char c : rest) {
        if (!isWordByte(c))
            return false;
    }
    database = rest;
    return true;
}

std::vector<std::pair<std::string, std::string>> parseSetAssignments(const std::string & sql) {
    const auto i = skipWhitespaceAndComments(sql, 0) + 3;
    std::string body = trim(std::string_view(sql).substr(std::min(i, sql.size())));
    if (!body.empty() && body.back() == ';')
        body.pop_back();
    std::vector<std::pair<std::string, std::string>> out;
    auto add = [&](std::string_view part) {
        const auto eq = part.find('=');
        if (eq == std::string_view::npos)
            return;
        auto value = trim(part.substr(eq + 1));
        if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'')
            value = value.substr(1, value.size() - 2);
        out.emplace_back(trim(part.substr(0, eq)), value);
    };
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t j = 0; j < body.size(); ++j) {
        const char c = body[j];
        if (c == '\'' || c == '"' || c == '`')
            j = skipQuoted(body, j) - 1;
        else if (c == '(' || c == '[')
            ++depth;
        else if (c == ')' || c == ']')
            --depth;
        else if (c == ',' && depth == 0) {
            add(std::string_view(body).substr(start, j - start));
            start = j + 1;
        }
    }
    add(std::string_view(body).substr(start));
    return out;
}

} // namespace runner
