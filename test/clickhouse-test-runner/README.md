# clickhouse-test-runner

This harness runs the ClickHouse core SQL tests
([`tests/queries/0_stateless`](https://github.com/ClickHouse/ClickHouse/tree/master/tests/queries/0_stateless))
through the ODBC driver of this repository.

The upstream test runner
([`tests/clickhouse-test`](https://github.com/ClickHouse/ClickHouse/blob/master/tests/clickhouse-test))
runs each `.sql` test through `clickhouse-client` and compares the output with
the `.reference` file of the test. This harness is a small C++ program that
imitates `clickhouse-client` well enough for the runner to drive it without
changes: the same flags, the same stdin and `--query` behavior, and the
`extract-from-config` shortcut.

The program is an ODBC application. It sends every statement through the
driver with the ODBC API of the driver manager (unixODBC), reads each value
with `SQLGetData` and the C type of the SQL type that the driver reports, and
writes the values as `TabSeparated` text. Thus a test passes only when the
driver reads, converts and describes every value correctly.

It is a port of the Go harness
([ClickHouse/clickhouse-go#2141](https://github.com/ClickHouse/clickhouse-go/pull/2141),
directory `tests/clickhouse-test-runner`), which is a port of the
[Java](https://github.com/ClickHouse/clickhouse-java/tree/main/tests/clickhouse-client)
and [JavaScript](https://github.com/ClickHouse/clickhouse-js/tree/main/tests/clickhouse-test-runner)
harnesses.

## Run the tests locally

You need the build tools of the driver (see
[Building from sources](../../README.md#building-from-sources-debianubuntu)),
the unixODBC development files, CMake, a C++20 compiler (Clang 18 or GCC 11
or later), Python 3 with `jinja2`, and Docker. The commands run from the root
of the repository, on Linux.

1. Build the driver. The harness uses the ANSI driver,
   `build/driver/libclickhouseodbc.so`. Build it as
   [`Linux.yml`](../../.github/workflows/Linux.yml) does, for example:

   ```bash
   CC=clang-18 CXX=clang++-18 cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_ASM_COMPILER=clang-18 -DODBC_PROVIDER=UnixODBC
   cmake --build build --parallel 8 --target clickhouse-odbc
   ```

   The harness does not change the driver build: the program is a separate
   CMake project, which the root `CMakeLists.txt` does not include. The
   program loads the driver library at run time (`Driver=` in the connection
   string), so you build the driver again only when `driver/` changes.

2. Start the pinned ClickHouse server, with the default configuration of the
   server image. The version is in [`clickhouse-version`](clickhouse-version):

   ```bash
   docker run -d --name clickhouse -p 8123:8123 -p 9000:9000 \
     --hostname "$(hostname)" -e CLICKHOUSE_SKIP_USER_SETUP=1 --ulimit nofile=262144:262144 \
     "clickhouse/clickhouse-server:$(test/clickhouse-test-runner/scripts/pinned-version.sh)"
   ```

   `CLICKHOUSE_SKIP_USER_SETUP=1` keeps the `default` user of the image, with
   network access and access management. The host name: the runner replaces
   its own host name with `localhost` in the output of each test, and some
   `.reference` files have `localhost` for `hostName()`.

   When the server answers, flush its logs once. On a new server, the system
   log tables (`system.query_log` and others) exist only after the first
   flush, and some tests read them:

   ```bash
   docker exec clickhouse clickhouse-client -q 'SYSTEM FLUSH LOGS'
   ```

3. Get the upstream tests of the same version:

   ```bash
   test/clickhouse-test-runner/scripts/fetch-upstream.sh
   ```

   The script checks out the release tag of the pinned version (for example
   `v26.10.4.207-stable`) into `.upstream/ClickHouse`, sparse and shallow.
   The `.reference` files of that tag were made by the same server build. The
   script also generates `src/errorcodes.cpp` from the same checkout. To try
   another version, give it as the argument (`fetch-upstream.sh 26.11.1.10`),
   and start the server of that version.

   The run script stops when the server and the tests are of different
   versions, because then the `.reference` files do not match. Set
   `ALLOW_VERSION_MISMATCH=1` to run anyway.

4. Run the allowlisted tests:

   ```bash
   pip install jinja2
   test/clickhouse-test-runner/scripts/run-upstream-tests.sh -j 8
   ```

The script builds the program into `test/clickhouse-test-runner/.build/`
(CMake, with Ninja when it is installed), links it as `.bin/clickhouse` and
`.bin/clickhouse-client` in the same directory, puts `.bin` first in `PATH`
for the runner only, and starts `tests/clickhouse-test`.
It gives the runner
`--no-stateful --no-random-settings --no-random-merge-tree-settings`; other
arguments go to the runner after these. For example, to run one test (also a
test that is not in the allowlist):

```bash
UPSTREAM_TEST_LIST=<(echo 00001_select_1) test/clickhouse-test-runner/scripts/run-upstream-tests.sh
```

The runner shows the diff of a failed test with the `.reference` file in its
own output, and keeps the output of the program next to the test, in
`.upstream/ClickHouse/tests/queries/0_stateless/<test>.<pid>.stdout` and
`.stderr` (`<test>.stdout` for a test that runs alone).

The unit tests of the program need no server. The live tests compare the
output of the program with the server's own `TabSeparated` output for many
expressions, and need the driver and the server of steps 1 and 2:

```bash
cmake -S test/clickhouse-test-runner -B test/clickhouse-test-runner/.build
cmake --build test/clickhouse-test-runner/.build
test/clickhouse-test-runner/.build/clickhouse-test-runner-unit-tests
python3 test/clickhouse-test-runner/live_test.py -v
```

The unit tests use the googletest of `contrib/` (`git submodule update --init`).
`-DCLICKHOUSE_TEST_RUNNER_UNIT_TESTS=OFF` builds the program without them.

### Environment variables

The default paths are in `test/clickhouse-test-runner/`, except the driver,
which is in the build directory of the repository.

| Variable | Default | Description |
|---|---|---|
| `CLICKHOUSE_TEST_RUNNER_DRIVER` | `build/driver/libclickhouseodbc.so` | The driver library. |
| `CLICKHOUSE_TEST_RUNNER_BACKEND` | `odbcdriver2` | The result format that the driver reads: `odbcdriver2` or `rowbinary` (see [The `rowbinary` backend](#the-rowbinary-backend)). |
| `UPSTREAM_CLICKHOUSE_DIR` | `.upstream/ClickHouse` | The checkout of `ClickHouse/ClickHouse`. |
| `UPSTREAM_REPO_URL` | the GitHub repository | The repository that `fetch-upstream.sh` fetches from. |
| `UPSTREAM_TEST_LIST` | `allowlist.txt` | The file with the tests to run. |
| `KNOWN_FAILURES` | (none) | Run the known failures of these groups instead of the allowlist: comma-separated IDs, or `all`. |
| `SHARD_INDEX`, `SHARD_TOTAL` | `1`, `1` | Run only shard `SHARD_INDEX` of `SHARD_TOTAL` round-robin shards of the list. |
| `RETRY_FAILED` | `0` | Run the failed tests again, at most this number of times. CI does not use it. |
| `ALLOW_VERSION_MISMATCH` | `0` | `1` runs the tests also when the server and the tests are of different versions. |
| `CLICKHOUSE_CLIENT_CLI_LOG` | (none) | A file. Each invocation of the program adds its arguments, the connection string, the statements and the errors to it. |
| `CLICKHOUSE_HOST` | `localhost` | The server host for the runner, the version check and the live tests. |
| `CLICKHOUSE_PORT_TCP`, `CLICKHOUSE_PORT_HTTP` | `9000`, `8123` | The ports of the server. When `CLICKHOUSE_PORT_TCP` is set, the runner gives `--port=$CLICKHOUSE_PORT_TCP`; the program uses `CLICKHOUSE_PORT_HTTP` for that port and when there is no `--port`. |
| `CLICKHOUSE_CONTAINER` | `clickhouse` | The server container for `run-with-real-client.sh`. |
| `CLICKHOUSE_TEST_RUNNER_BIN` | `.build/clickhouse-test-runner` | The program that `live_test.py` runs. |

The driver (libcurl) uses the proxy of the environment. The run script adds
the server host to `NO_PROXY`.

### Check the format of the code

The C++ sources follow the [`.clang-format`](../../.clang-format) of the
repository. clang-format 18 refuses that file, because two keys occur two times
(`ConstructorInitializerIndentWidth`, `IndentWidth`); older versions use the
last value. Check with a copy that keeps the last value of each key:

```bash
mkdir -p /tmp/cf
tac .clang-format | awk '/^[A-Za-z]/ { if (seen[$1]++) next } { print }' | tac > /tmp/cf/.clang-format
clang-format --style=file:/tmp/cf/.clang-format --dry-run --Werror \
  test/clickhouse-test-runner/src/*.cpp test/clickhouse-test-runner/src/*.h \
  test/clickhouse-test-runner/tests/*.cpp
```

`scripts/gen-error-codes.sh` writes `src/errorcodes.cpp` in this format.

## How the program uses the driver

The program uses the driver as an application does. It connects with
`SQLDriverConnect` (`Driver=<library>;Url=...;UID=...;PWD=...;Database=...`,
no DSN), runs each statement with `SQLExecDirect`, and when the result has
columns, reads the metadata of each column (`SQLDescribeCol`, and
`SQLColAttribute` for `SQL_DESC_TYPE_NAME`, `SQL_DESC_UNSIGNED`,
`SQL_DESC_PRECISION` and `SQL_DESC_SCALE`), then the rows (`SQLFetch`,
`SQLGetData`). An error comes from `SQLGetDiagRec`.

### The C type of each SQL type

The renderer is type-directed: the SQL type and the type name that the driver
reports select the C type that the program reads with, and the text that it
writes. It writes the text as the server writes it for `TabSeparated` (for
example, the shortest float text that reads back as the same value, `nan`,
`inf`, `-0`; decimals without trailing zeros; `DateTime64(P)` with P fraction
digits). A combination that the renderer does not know is an error, so a test
cannot pass by accident.

| ClickHouse type | SQL type (driver) | C type (program) |
|---|---|---|
| `Bool` | `SQL_BIT` | `SQL_C_BIT` (`true`, `false`) |
| `Int8`, `UInt8` | `SQL_TINYINT` | `SQL_C_STINYINT`, `SQL_C_UTINYINT` |
| `Int16`, `UInt16` | `SQL_SMALLINT` | `SQL_C_SSHORT`, `SQL_C_USHORT` |
| `Int32` | `SQL_INTEGER` | `SQL_C_SLONG` |
| `UInt32`, `Int64`, `UInt64` | `SQL_BIGINT` | `SQL_C_UBIGINT`, `SQL_C_SBIGINT`, `SQL_C_UBIGINT` |
| `Float32` | `SQL_REAL` | `SQL_C_FLOAT` |
| `Float64` | `SQL_DOUBLE` | `SQL_C_DOUBLE` |
| `Decimal(P, S)`, P ≤ 38 | `SQL_DECIMAL` | `SQL_C_NUMERIC`, read as `SQL_ARD_TYPE` with the precision and scale of the column |
| `Decimal(P, S)`, P > 38 | `SQL_DECIMAL` | `SQL_C_CHAR`: `SQL_NUMERIC_STRUCT` holds at most 38 digits |
| `Date`, `Date32` | `SQL_TYPE_DATE` | `SQL_C_TYPE_DATE` |
| `Time` | `SQL_TYPE_TIME` | `SQL_C_TYPE_TIME` |
| `DateTime`, `DateTime64(P)` | `SQL_TYPE_TIMESTAMP` | `SQL_C_TYPE_TIMESTAMP` (P from `SQL_DESC_SCALE`) |
| `UUID` | `SQL_GUID` | `SQL_C_GUID` |
| `String`, `FixedString(N)` | `SQL_VARCHAR` | `SQL_C_CHAR`, in parts of 64 KiB |
| all other types | `SQL_VARCHAR` | `SQL_C_CHAR` |

`Nullable(T)` and `LowCardinality(T)` have the SQL type of `T`; a NULL is
`SQL_NULL_DATA`, written as `\N`. The signedness of an integer comes from
`SQL_DESC_UNSIGNED`.

### The types that reach the application as text

For these types the driver reports `SQL_VARCHAR` with the type name `String`,
and gives the server's text of the value: `Int128`, `UInt128`, `Int256`,
`UInt256`, `BFloat16`, `Time64`, `Enum8`, `Enum16`, `IPv4`, `IPv6`, `Array`,
`Tuple`, `Map`, `Interval*`, `JSON`, `Variant`, `Dynamic`, the geo types,
`QBit`, `AggregateFunction` and other composite types. `Decimal(P, S)` with P > 38 is `SQL_DECIMAL`, but the program
reads it as text (see the table).

The value is correct, but an application cannot tell an `Array(String)` from
a `String`, because the type name is the base name only (also for
`Nullable(T)`, `LowCardinality(T)`, `Decimal(P, S)` and `DateTime64(P)`). The program writes such a
value by the rules of a `String`: it escapes the quotes of the elements, so
`['a']` becomes `[\'a\']`, and the test fails. The program does not guess the
type from the text: that would hide the bug. These tests are the known failures
of `[type-name-string]` in [`allowlist.txt`](allowlist.txt).

### Why the ANSI driver

On Linux an application calls the `char` functions of the driver manager with
UTF-8 text. The ANSI driver (`libclickhouseodbc.so`) gives the bytes of a
`String` to the application as the server sent them. The Unicode driver
(`libclickhouseodbcw.so`) converts text to UTF-16, so a `String` that is not
valid UTF-8 (the tests have many) cannot come through it unchanged.

### Session, settings and the database

- **Session.** All requests of one invocation carry the same `session_id`,
  which the program gives in the query string of `Url`. The driver sends the
  query string of `Url` with each request, so `SET`, `USE` and temporary tables
  last for the whole invocation, also after an error. The program does not use
  the `AutoSessionId` option of the driver: it makes a new `session_id` for
  each connection, and the program must connect again after some `SET`
  statements (below). Without a `session_id` (the default of the driver), each
  statement runs in a new session.
- **Settings.** Of the settings on the command line, only `log_comment`,
  `max_insert_threads`, `max_threads` and `send_logs_level` go to the server,
  in the query string of `Url`, so with each query, as in `clickhouse-client`.
  A `SET` of the same setting in the test replaces the command-line value: the
  program then connects again, with the same `session_id` and without that
  setting in the URL. `--param_<name>=<value>` gives a query parameter
  (`param_<name>` in the URL).
- **Database.** The driver sends the database of the connection with each
  request, and that database replaces the current database of the session.
  After a `USE`, the program sets `SQL_ATTR_CURRENT_CATALOG` to the new
  database, as `clickhouse-client` sets its current database.
- **Timeout.** The connection string has `Timeout=300`: `clickhouse-client`
  waits 300 s for a response (`receive_timeout`), and the default of the driver
  is 30 s.

### The `rowbinary` backend

`CLICKHOUSE_TEST_RUNNER_BACKEND=rowbinary` gives
`default_format=RowBinaryWithNamesAndTypes` in the URL, so the driver reads
that format instead of `ODBCDriver2`. The driver documents it as experimental
(see [URL query string](../../README.md#url-query-string)). The backend is for
work on the driver; CI does not run it, and the allowlist is for
`odbcdriver2` only. On 26.10.4.207, with this format: integers, `String` and
`Date` are correct; an `Array` reads as an empty value without an error;
`Int128`, `Bool`, `UUID`, `IPv4` give `unknown type id`; `Enum8` gives
`Unable to read values of an unknown type`; `DateTime64` fractions read as
`.000`, and a `DateTime64` before 1970 fails; `Date32`, `LowCardinality`,
`Tuple`, `Map`, `Interval`, `Time` and `Decimal` fail. The driver also
converts date and time values to the local time zone with this format.

## How the program imitates clickhouse-client

The program sends nothing that `clickhouse-client` does not send, because
some tests read their own queries from `system.query_log`. The driver sends no
queries of its own for `SQLExecDirect`.

- **Statements.** The program splits the input into statements as
  `clickhouse-client` does: at a top-level `;`, with the same-line trailing
  comment kept with its statement. It sends each statement with its `;`. The
  inline data of `INSERT ... FORMAT <name>` (other than `Values`) ends at an
  empty line; `INSERT ... SELECT ... FROM input(...) FORMAT <name>` has inline
  data too. The `-- Tags:` header is not part of a statement. Every statement
  runs, with or without `--multiquery`.
- **Test hints.** `-- { serverError X }`, `-- { clientError X }` and
  `-- { error X }` (codes or names) make the error the expected result. A hint
  in a leading comment is ignored, as upstream. Through the driver the server
  finds all errors, so a server error can satisfy a `clientError` hint.
  `-- { echo }`, `-- { echoOn }` and `-- { echoOff }` print the statements,
  but not a statement with a syntax error (`clickhouse-client` parses before
  it echoes). `-- { retry ... }` is not supported.
- **Errors.** The driver reports a server error with SQLSTATE `HY000`
  (`28000` for an authentication error) and the native error 1; the program
  reads the ClickHouse error code from `Code: N.` in the message. An unexpected
  error stops the program. It writes the error with its code and name to
  stderr, and the exit code is the error code modulo 256 (1 when that is 0, or
  when the error has no code).
- **Other dialects.** After `SET dialect` to another language (KQL, PRQL, ...)
  the program removes the trailing `;` of each statement: over HTTP the server
  reads the request body as one query, and the parsers of these dialects do not
  accept it. The `let` statements of a Kusto query are part of the query.
- **Output formats.** The driver reads only its own result format, and fails
  on any explicit `FORMAT` clause. So the program removes a top-level
  `FORMAT <name>` clause of the `TabSeparated` family (also `TSVRaw` and
  `...WithNames[AndTypes]`) and writes the result in that format itself. It
  sends a `FORMAT Null` statement unchanged (the server then logs the text of
  the test), and does not write its rows. When the statement expects an error,
  the program removes `FORMAT Null`, reads the rows and does not write them:
  `clickhouse-client` applies `FORMAT Null` itself, after the server sent the
  result, so an error of the output stage occurs. A statement with any other output
  format fails (`[shim-output-format]`). The word `FORMAT` is the clause only at
  the end of a query (before an optional `SETTINGS`), when the word after it is
  not a SQL keyword: `SELECT format FROM t` has no clause.
- **The command line.** `--port` is the native port of the runner
  (`CLICKHOUSE_PORT_TCP`); the program maps it to `CLICKHOUSE_PORT_HTTP`.
  `clickhouse extract-from-config --key listen_host` prints `127.0.0.1`.
  `--help` shows the options.
- **Error names.** [`src/errorcodes.cpp`](src/errorcodes.cpp) maps the error
  names of test hints to codes.
  [`scripts/fetch-upstream.sh`](scripts/fetch-upstream.sh) generates it from
  `src/Common/ErrorCodes.cpp` of the upstream checkout, so it always matches
  the tests. Commit the file when it changes.

## The allowlist

[`allowlist.txt`](allowlist.txt) lists the tests that pass reliably, one test
name (the file name without `.sql`) per line. `#` starts a comment. The script
gives the runner anchored regular expressions, so each name selects exactly
one test. (The runner itself matches each name as a substring, so
`00396_uuid` would also select `00396_uuid_v7`.)

The file has three kinds of lines:

| Line | Meaning |
|---|---|
| `00001_select_1` | The test runs with every backend that CI runs (`odbcdriver2`). |
| `<test>  # only=odbcdriver2: <reason>` | The test runs with that backend only, because it cannot pass with another backend for an environment cause. The file has no such lines now. |
| `# 00007_array  # [type-name-string]` | A **known failure**: the test does not run, because of a bug in the driver or a gap in the program. The ID in brackets names the group. The header of the group, near the end of the file, describes the bug, gives a small reproduction with the ODBC API and has an `Issue:` line. |

A group header starts with the group ID and `client:` (a bug in the driver) or
`shim:` (a gap in this program).

To run the known failures of one or more groups, for example to check a fix:

```bash
KNOWN_FAILURES=float-subnormal test/clickhouse-test-runner/scripts/run-upstream-tests.sh
KNOWN_FAILURES=all test/clickhouse-test-runner/scripts/run-upstream-tests.sh --max-failures-chain 1000000
```

When a fix makes the tests of a group pass, remove the `# ` at the start and
the `# [<id>]` at the end of their lines. When all tests of a group pass,
remove the group header.

### Grow the allowlist

1. Run the candidate tests through the harness. To try all `.sql` tests:

   ```bash
   ls test/clickhouse-test-runner/.upstream/ClickHouse/tests/queries/0_stateless \
     | sed -nE 's/\.sql(\.j2)?$//p' | sort -u > /tmp/candidates.txt
   UPSTREAM_TEST_LIST=/tmp/candidates.txt \
     test/clickhouse-test-runner/scripts/run-upstream-tests.sh -j 6 --no-long --timeout 120 \
       --max-failures-chain 1000000 | tee /tmp/run.log
   ```

   Run one such sweep at a time on a server: the memory pressure of two
   causes `MEMORY_LIMIT_EXCEEDED` failures that are not real.
   `--max-failures-chain` turns off a limit of the runner: by default it stops
   after 20 failures in a row, which a list of failing tests reaches at once.

2. Keep only the tests that report `[ OK ]`:

   ```bash
   sed -nE 's/^.*\] ([^ :]+): +\[ OK \].*/\1/p' /tmp/run.log | sort -u
   ```

3. Run the new tests again, three times, with `-j` higher than CI uses, on a
   **fresh** server, and remove the tests that do not pass every time. Tests
   change the state of the server, and CI starts from a fresh one.

4. For each test that fails, find the cause before you decide (see
   [`AGENTS.md`](AGENTS.md)). A test that fails because of a bug in the driver
   or a gap in this program goes in a known-failure group. A test that fails
   for an environment cause is not listed.

**Rule:** add only tests that pass reliably. Before you remove a test or
decide not to add it, find out whether it fails because of a bug in the
driver. The harness exists to find those bugs. See [`AGENTS.md`](AGENTS.md)
for the steps.

To compare with the real `clickhouse-client`, run the same tests through it,
in the server container (`CLICKHOUSE_CONTAINER`, default `clickhouse`):

```bash
UPSTREAM_TEST_LIST=<(echo 00007_array) \
  test/clickhouse-test-runner/scripts/run-with-real-client.sh
```

A test that passes there and fails through the driver shows a bug in the
driver or in this program.

## Known failures

The end of [`allowlist.txt`](allowlist.txt) lists the tests that fail
because of a bug in the driver or a gap in this program, in groups. Each group
header describes the cause and gives a small reproduction that uses only the
ODBC API. A test that cannot pass for an environment cause (a cluster,
ZooKeeper, server configuration, features of `clickhouse-client` itself, the
`ODBCDriver2` format) is not listed; see
[`AGENTS.md`](AGENTS.md#environment-causes). The program does not try `.sh`
tests.

## CI

[`upstream-sql-tests.yml`](../../.github/workflows/upstream-sql-tests.yml)
runs the allowlist nightly, manually, and on pushes to `master` and pull
requests that change this directory or the workflow.

- The `build` job builds the driver once, as the Release/Clang leg of
  `Linux.yml` does, and runs the unit tests of the program with Clang 18 and
  with the runner's `g++` (GCC 11). A compiler warning fails these builds.
- The matrix of the test jobs is `{backend: odbcdriver2} × {shard: 1..4}`.
  Each job takes the driver library of the `build` job, builds the program
  with `g++`, starts the server, flushes its logs once (see "Run the tests
  locally"), and runs its shard at `-j 4`. Shard 1 also runs the live tests.
  A shard is about 2,300 tests and takes about 3 minutes on a 4-vCPU runner. If a shard takes much more than a few minutes, increase
  the number of shards: change the `shard` list and `SHARD_TOTAL` together.
- CI does not run failed tests again (`RETRY_FAILED`). No known bug makes
  random tests fail.
- The server version and the upstream tests are pinned together in
  [`clickhouse-version`](clickhouse-version). A new ClickHouse release
  changes nothing in CI, so a pull request never fails because of a release
  that it does not touch. A bump is a pull request of its own; see
  [`AGENTS.md`](AGENTS.md#bump-the-server-version). For the pinned version,
  the test jobs fail when the committed `src/errorcodes.cpp` is not the same
  as the file that `fetch-upstream.sh` generates.
- The `clickhouse_version` input of a manual run tries another version
  without a commit, for example before a bump.
- When an allowlisted test is not in the checkout (upstream renamed or
  removed it), the run shows a warning with the names.
- When a job fails, it uploads the output of the runner (with the diff of
  each failed test), the `.stdout` and `.stderr` files of the failed tests, and
  the log of the program (`CLICKHOUSE_CLIENT_CLI_LOG`).
