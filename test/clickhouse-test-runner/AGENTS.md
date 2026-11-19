# Instructions for agents: upstream SQL test harness

This harness runs the ClickHouse core SQL tests through the ODBC driver of this repository. See
[`README.md`](README.md) for how it works and how to run it.

## The goal

The goal is to find bugs in the driver. To pass more tests is not the goal. A failed test is
information. Do not hide it.

## Rules

1. **Find the cause of a failure before you do anything with the test.** A test fails for one of
   three causes:
   - **The client**: the driver (`driver/`), as an ODBC application sees it through unixODBC. This
     is what the harness is for.
   - **The shim** (the program in this directory). It does not do what `clickhouse-client` does.
   - **The environment.** The test cannot pass through any client on this server (see
     [Environment causes](#environment-causes)).
2. **The default for a client bug is a fix in the driver**, in a pull request of its own, with a
   test in `driver/test/` or `test/src/e2e/`. Do not fix a driver bug in a pull request that
   changes the harness.
3. **The default for a shim bug is a fix in the shim.** Make the shim copy `clickhouse-client`
   more exactly. Do not change the shim to hide a client bug, for example with a different C type,
   a guess of the ClickHouse type from the text of a value, or a workaround for a wrong value. The
   shim must use the driver as an application does: read each value with `SQLGetData` and the C
   type of the SQL type that the driver reports, and write it by the type that the driver reports
   (see "How the program uses the driver" in the README).
4. **Remove a test from the allowlist only when it cannot pass for an environment cause** (see
   rules 6 and 7). Write the reason in the commit message.
5. **A test that fails because of a client bug or a shim gap stays visible.** Do not delete it.
   Put it in [`allowlist.txt`](allowlist.txt) as a known failure, `# <test>  # [<id>]`, in the
   group of its cause. Each group has a header that describes the bug, gives a small reproduction
   and an `Issue:` line. Make a new group when no group fits. See the README for the format and
   for how to run one group (`KNOWN_FAILURES=<id>`).
6. **A test that cannot pass with one backend** for an environment cause gets
   `# only=<backend>: <reason>`. Do not use this for a client bug: use a known failure. (CI runs
   one backend now, `odbcdriver2`.)
7. **Do not list a test that cannot pass for an environment cause.**
8. **A test that fails because of a vulnerability that is not public yet** is not listed, and its
   cause is not described in this directory, in a commit message or in a pull request. Report it
   privately (**Security** → **Report a vulnerability**), as the root [`AGENTS.md`](../../AGENTS.md)
   says. Do not add a workaround for it to the shim.

## How to find the cause

Do these steps in this order.

1. **Run the test through the harness:**

   ```bash
   UPSTREAM_TEST_LIST=<(echo <test>) test/clickhouse-test-runner/scripts/run-upstream-tests.sh
   ```

2. **Run the test through the real `clickhouse-client`** in the server container, on the same
   server:

   ```bash
   UPSTREAM_TEST_LIST=<(echo <test>) test/clickhouse-test-runner/scripts/run-with-real-client.sh
   ```

   If it also fails here, the cause is the environment. If it passes here, the cause is the client
   or the shim.
3. **Find the statement that fails.** Set `CLICKHOUSE_CLIENT_CLI_LOG` to a file; the log shows
   each statement, the URL and the error. Read the diff in the output of the runner, and the
   `.stdout` and `.stderr` files next to the test.
4. **Compare the shim with the server on that one statement.** Run it through the shim
   (`test/clickhouse-test-runner/.build/clickhouse-test-runner -q "<SQL>"`, with
   `CLICKHOUSE_TEST_RUNNER_DRIVER` set) and through `curl` with `FORMAT TSV`. Then make the
   statement smaller until it is as small as possible.
5. **Decide between the client and the shim with a small program** that uses only the ODBC API
   (`SQLDriverConnect`, `SQLExecDirect`, `SQLDescribeCol`, `SQLColAttribute`, `SQLGetData`). If
   the program also gives a wrong value, wrong metadata or an error, the bug is in the client. If
   the program gives the correct value, the bug is in the shim.

### Environment causes

A test cannot pass through any client on the CI server when it needs one of these:

- A cluster, shards or replicas (`CLUSTER_DOESNT_EXIST`, `test_shard_localhost`).
- ZooKeeper or Keeper (`NO_ZOOKEEPER`, `Replicated*` engines).
- External services: S3, Azure, HDFS, Kafka, MySQL, PostgreSQL, MongoDB, URL sources.
- Server configuration of the upstream CI: named collections, dictionaries, storage policies,
  disks, users and grants, macros (`NAMED_COLLECTION_DOESNT_EXIST`, `NO_ELEMENTS_IN_CONFIG`,
  `UNKNOWN_POLICY`, `UNKNOWN_DISK`).
- Features that the server build or platform does not have (`SUPPORT_IS_DISABLED`).
- Stateful datasets (`--no-stateful`) or long tests (`--no-long`).
- Behavior of the HTTP interface and of the result format itself. The `ODBCDriver2` format has no
  `extremes`, and it writes the `WITH TOTALS` row as one more data row, without a marker. An HTTP
  request fails when the current database of the session was dropped. Some log tables and profile
  events are different for a query over HTTP. The memory that a query uses over HTTP is different
  from TCP, and it changes with the output format: a test that expects `MEMORY_LIMIT_EXCEEDED` at a
  small `max_memory_usage` that is set for the native protocol can pass on one server and fail on
  another (`04101_variant_adaptor_propagate_exceptions`).
- Features of `clickhouse-client` itself that are not part of a driver: `INTO OUTFILE`,
  `FROM INFILE`, client-side parsing of `INSERT` data, the progress and the logs that the native
  protocol sends, and client-only settings.
- Tests that conflict with other tests of the set: a fixed table UUID, or server state that a test
  leaves behind (failpoints that a failed test did not disable, row policies). Such a test passes
  alone on a fresh server and fails in a full run. A failure message with "Injected failure"
  comes from a failpoint of another test.
- Memory limits of the server, when the test also fails through the real `clickhouse-client`.
- Results that depend on the number of threads that the server gives a query, which changes under
  load: the width and the steps of `EXPLAIN PIPELINE` and `EXPLAIN ANALYZE`, the number of read
  streams, and plans after an `INSERT ... SELECT` from `numbers_mt`. Such a test passes alone and
  fails now and then in a full run, also at `-j 4`.
- A race in the server over HTTP: a statement without output (`... FORMAT Null`) right before
  `SYSTEM FLUSH LOGS`, and then a read of `system.query_log`. The response of the first request
  can end before the server adds the `QueryFinish` row, so the flush of the next request does not
  include it. Plain `curl` shows the same race; the real client (TCP) does not.
- A race in the test itself, for example `ATTACH TABLE ... UUID` right after the `DROP TABLE` of
  the same UUID (the server removes the UUID in a background thread).
- `hostName()`: the runner replaces its own host name with `localhost` in the output. CI starts
  the server with the host name of the runner; a server with another host name fails these tests.

These are **not** environment causes. Find the bug:

- A value that is different from the server's value.
- An error of the driver (an SQLSTATE without a ClickHouse error code), for example
  `Cannot interpret '...' as ...` from `SQLGetData`.
- Metadata that does not describe the column (`SQL_DESC_TYPE_NAME`, precision, scale).
- A query text that the driver changed before it sent it (parameter markers, escape sequences).

### Tests that are left out for load

The checks of the allowlist (three full runs on a fresh server at `-j 6`) found the load-dependent
classes above. They are left out whole, not one test at a time, because each test of a class has
the same dependency, and a check finds only a few of them in each run:

- the tests without the `no-parallel` tag that use `EXPLAIN PIPELINE` or `EXPLAIN ANALYZE` (match
  the tags as a list: the tag `no-parallel-replicas` contains the text `no-parallel`);
- the tests that set `max_streams_to_max_threads_ratio`, `max_streams_multiplier_for_merge_tables`
  or `max_streams_for_merge_tree_reading`;
- the tests with a `... FORMAT Null` statement right before `SYSTEM FLUSH LOGS`.

To try such a test again, run it many times under the load of a full run: a stress run of a small
list does not load the server as a full run does. For example, `--test-runs 10` with the test in
`UPSTREAM_TEST_LIST`, and then three full checks on a fresh server. Add it only when it passes
every time.

## Conditions for the allowlist

- Add a test only when it passes with every backend that CI runs, or with one backend with an
  `only=` reason (rule 6).
- Run new tests three times, also with a higher `-j` than CI, on a **fresh** server. Tests change
  the state of the server (row policies, settings profiles, users). A test that passes only after
  other tests ran must not go in.
- Do not run two sweeps at the same time on one server. The memory pressure causes
  `MEMORY_LIMIT_EXCEEDED` failures that are not real.
- CI does not run failed tests again (`RETRY_FAILED`), because no known driver bug makes random
  tests fail. Do not use the retry when you grow or check the allowlist: a test that needs a retry
  is not reliable.
- When a test in the allowlist starts to fail intermittently, find the cause first (rule 1).
  Remove it only when the cause is in the environment or in the test itself, and write the
  reason.

## Bump the server version

The server version and the upstream tests are pinned together in
[`clickhouse-version`](clickhouse-version). A new ClickHouse release changes nothing in CI until a
pull request bumps the pin, so other pull requests do not fail because of a release. Bump the pin
in a pull request of its own, and make that pull request pass before it merges:

1. Select the version: a full version with a server image tag and a `v<version>-stable` tag
   upstream, normally the version of the `latest` image.
2. Try it: start a manual run with the `clickhouse_version` input, or run the allowlist locally on
   that version (`fetch-upstream.sh <version>`, and a fresh server of that version).
3. For each allowlisted test that now fails, find the cause (rule 1). A release changes the tests
   and the server output, so many tests can fail at the same time with no change in the driver. To
   see whether the driver is the cause, run the same test through the real client
   (`run-with-real-client.sh`) on the same server.
4. Run the known failures (`KNOWN_FAILURES=all`). Enable the tests that pass now.
5. Fix the names of allowlisted tests that the run reports as not in the checkout (upstream
   renamed or removed them).
6. Optionally, try the new tests of the release (see "Grow the allowlist" in the README).
7. Change `clickhouse-version`, run `fetch-upstream.sh`, and commit `src/errorcodes.cpp` with it.

## Keep the documents in sync

When you change the behavior of the shim (flags, statement handling, output formats, settings,
the C types), update [`README.md`](README.md), this file, the live test and the workflow in the
same change.

## Before a pull request

- Build the shim with Clang 18 and with GCC 11 or later (CI builds it with the `g++` of the
  runner, GCC 11) with no warnings, and run the unit tests with each build. The `build` job of CI
  builds with `-DCMAKE_COMPILE_WARNING_AS_ERROR=ON`.
- Run the live test (`live_test.py`) against the pinned server.
- Check the format of the C++ sources with clang-format (README, "Check the format of the code").
- Do not write a URL with a variable host as one string in a script: build it from parts, as
  `"http"'://'"${host}:${port}/"`. A link checker reads such a text as a link.
