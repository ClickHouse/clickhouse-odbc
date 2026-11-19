#!/usr/bin/env python3
"""Live tests of the shim: they need the driver, the built shim and a ClickHouse server.

The main test compares, for many expressions, the TabSeparated text that the shim writes from the
values that the driver returned with the server's own TabSeparated text. Other tests check the
session, the errors and the echo hints.

    python3 test/clickhouse-test-runner/live_test.py -v

Environment: CLICKHOUSE_HOST (default localhost), CLICKHOUSE_PORT_HTTP (default 8123),
CLICKHOUSE_TEST_RUNNER_DRIVER (default build/driver/libclickhouseodbc.so),
CLICKHOUSE_TEST_RUNNER_BIN (default .build/clickhouse-test-runner), CLICKHOUSE_TEST_RUNNER_BACKEND.
"""

import os
import subprocess
import unittest
import urllib.parse
import urllib.request

RUNNER_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(os.path.dirname(RUNNER_DIR))
HOST = os.environ.get("CLICKHOUSE_HOST", "localhost")
HTTP_PORT = os.environ.get("CLICKHOUSE_PORT_HTTP", "8123")
BIN = os.environ.get("CLICKHOUSE_TEST_RUNNER_BIN", os.path.join(RUNNER_DIR, ".build", "clickhouse-test-runner"))
DRIVER = os.environ.get("CLICKHOUSE_TEST_RUNNER_DRIVER", os.path.join(REPO_DIR, "build", "driver", "libclickhouseodbc.so"))

# Expressions for `SELECT <expr>`: the shim must write the same text as the server.
EXPRESSIONS = [
    "toInt8(-1), toUInt8(255), toInt16(-32768), toUInt16(65535), toInt32(-2147483648), toUInt32(4294967295)",
    "toInt64(-9223372036854775808), toUInt64(18446744073709551615)",
    "toInt128(-170141183460469231731687303715884105728), "
    "toUInt256(115792089237316195423570985008687907853269984665640564039457584007913129639935)",
    "arrayJoin([1e15, 1e21, 1.5e-7, 0.1 + 0.2, nan, inf, -inf, -0., 2.2250738585072014e-308])",
    "toFloat32(0.1), toFloat32(3.4028235e38), toFloat32(-0.), toFloat32(1.17549435e-38), toBFloat16(1.5)",
    "toDecimal32(1.50, 2), toDecimal64(-0.001, 5), toDecimal128(100, 3), toDecimal256('1.5', 10), toDecimal32(0, 3)",
    "toDecimal64('-999999999999999999', 0), toDecimal128('18446744073709551615', 0), toDecimal64('-0.000000001', 9)",
    "toDecimal256('-123456789012345678901234567890123456789012.5', 2)",
    "'a\\tb\\\\c\\nd''e\\0\\b\\f\\v\\x01', toFixedString('ab', 4), unhex('FF00C3'), 'é'",
    "true, false, NULL, CAST(NULL AS Nullable(String)), toNullable(1), CAST(NULL AS Nullable(UInt8))",
    "toDate('2020-01-02'), toDate('1970-01-01'), toDate('2149-06-06'), toDate32('1900-01-01'), toDate32('2299-12-31')",
    "toDateTime('2020-01-01 10:00:00', 'UTC'), toDateTime('2020-01-01 10:00:00', 'Asia/Tokyo'), toDateTime('1970-01-01 00:00:00', 'UTC')",
    "toDateTime64('2020-01-01 00:00:00.120', 3), toDateTime64('2020-01-01 00:00:00', 9, 'Asia/Tokyo')",
    "toDateTime64('1900-01-01 00:00:00.123456', 6, 'UTC'), toDateTime64('2262-04-11 23:47:16.854775807', 9, 'UTC')",
    "toDateTime64('1600-01-01 00:00:00', 3, 'UTC'), toDateTime64('2299-12-31 23:59:59.99', 2, 'UTC'), toDateTime64('2020-01-01 00:00:01', 0)",
    "toTime('12:34:56'), toTime('00:00:00'), toTime('23:59:59'), toTime64('12:00:00.5', 3), toTime64('-1:00:00.25', 2)",
    "toUUID('61f0c404-5cb3-11e7-907b-a6006ad3dba0'), toIPv4('1.2.3.4'), toIPv6('::ffff:1.2.3.4'), toIPv6('::1.2.3.4'), toIPv6('2001:db8::1')",
    "[1, 2], [[1], []], [], (1, 2.5), map(1, [2]), [nan, -inf], tuple()",
    "CAST('a' AS Enum8('a' = 1, 'b' = 2)), CAST('x\\ty' AS Enum16('x\\ty' = 1)), toLowCardinality('x'), toLowCardinality(toNullable('y'))",
    "INTERVAL 3 DAY, toIntervalSecond(-5)",
    "number, toString(number) FROM numbers(3)",
    "number % 2 AS k, count() FROM numbers(5) GROUP BY k ORDER BY k",
    # A column or alias named `format` is not the FORMAT clause.
    "1 AS format ORDER BY format DESC",
    "1 AS format ORDER BY format ASC",
    "format FROM (SELECT 1 AS format)",
    "1 AS format FROM numbers(1)",
    "1 AS format, format x",
    "3 AS format, 2 * format x",
]

# Expressions that show a known client bug (the group ID of allowlist.txt). They must still differ
# from the server's text; when one matches, the bug is fixed: move it to EXPRESSIONS.
KNOWN_CLIENT_BUGS = {
    # The driver reports these columns with SQL_DESC_TYPE_NAME `String`, so they are written by
    # the rules of a String: the quotes of the elements are escaped.
    "['a''b', NULL]": "type-name-string",
    "(1, 'x', NULL)": "type-name-string",
    "map('k', 1)": "type-name-string",
    "[toDate('2020-01-01')]": "type-name-string",
    "toFloat64(5e-324)": "float-subnormal",
    "toFloat32(1e-40)": "float-subnormal",
    "toDecimal128('18446744073709551616', 0)": "numeric-64bit",
    "toDecimal128('1.5', 30)": "numeric-64bit",
    "toTime('100:00:00')": "time-range",
    "toTime('-01:02:03')": "time-range",
    "makeDate32(0, 1, 2)": "year-zero",
    "toDateTime64(-62162035200.5, 1, 'UTC')": "year-zero",
    # Not a client bug: the server's ODBCDriver2 format writes the totals as one more data row, without
    # a marker (an environment class, not listed in allowlist.txt).
    "number % 2 AS k, count() FROM numbers(5) GROUP BY k WITH TOTALS ORDER BY k": "odbcdriver2-totals-as-row",
}

NO_PROXY = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def server_tsv(query):
    """The server's own TabSeparated text for a query."""
    url = "http" + "://" + HOST + ":" + HTTP_PORT + "/?" + urllib.parse.urlencode({"default_format": "TabSeparated"})
    with NO_PROXY.open(urllib.request.Request(url, data=query.encode(), method="POST"), timeout=60) as res:
        return res.read().decode("utf-8", "surrogateescape")


def run_shim(script, *args):
    """Runs the shim with the script on stdin. Returns (stdout, stderr, exit code)."""
    env = dict(os.environ)
    env["CLICKHOUSE_TEST_RUNNER_DRIVER"] = DRIVER
    env["CLICKHOUSE_PORT_HTTP"] = HTTP_PORT
    env["NO_PROXY"] = HOST + "," + env.get("NO_PROXY", "")
    env["no_proxy"] = HOST + "," + env.get("no_proxy", "")
    proc = subprocess.run(
        [BIN, "--host", HOST, *args], input=script.encode(), capture_output=True, env=env, timeout=120, check=False
    )
    return proc.stdout.decode("utf-8", "surrogateescape"), proc.stderr.decode("utf-8", "surrogateescape"), proc.returncode


class LiveTest(unittest.TestCase):
    def test_render_matches_server(self):
        for expr in EXPRESSIONS:
            with self.subTest(expr=expr):
                query = "SELECT " + expr
                got, stderr, code = run_shim(query)
                self.assertEqual(0, code, stderr)
                self.assertEqual(server_tsv(query), got)

    def test_known_client_bugs_still_differ(self):
        for expr, group in KNOWN_CLIENT_BUGS.items():
            with self.subTest(expr=expr, group=group):
                query = "SELECT " + expr
                got, _, code = run_shim(query)
                self.assertTrue(
                    code != 0 or got != server_tsv(query),
                    f"[{group}] is fixed for {query}: move the expression to EXPRESSIONS and update allowlist.txt",
                )

    def test_output_formats(self):
        got, stderr, code = run_shim("SELECT 1 AS x, 'a' AS y FORMAT TSVWithNames; SELECT 'a\\tb' FORMAT TSVRaw; SELECT 1 FORMAT Null; SELECT 2")
        self.assertEqual(0, code, stderr)
        self.assertEqual("x\ty\n1\ta\na\tb\n2\n", got)
        # A name that is also a keyword ends an operand: FORMAT after it is the clause.
        got, stderr, code = run_shim("SELECT 7 % 3 AS mod FORMAT TSVWithNames; SELECT number AS offset FROM numbers(1) ORDER BY offset FORMAT TSV")
        self.assertEqual(0, code, stderr)
        self.assertEqual("mod\n1\n0\n", got)
        _, stderr, code = run_shim("SELECT 1 FORMAT JSONEachRow")
        self.assertEqual(1, code)
        self.assertIn("output format JSONEachRow is not supported", stderr)

    def test_format_null(self):
        # The statement goes to the server unchanged: the server logs its text, and decides about
        # `FORMAT Null` of an EXPLAIN (ignore_format_null_for_explain).
        marker = "live_format_null_" + os.urandom(4).hex()
        script = "\n".join(
            [
                f"SELECT 1 AS {marker} FORMAT Null;",
                "EXPLAIN SYNTAX SELECT 1 FORMAT Null;",
                "SET ignore_format_null_for_explain = 0;",
                "EXPLAIN SYNTAX SELECT 1 FORMAT Null;",
                "SYSTEM FLUSH LOGS query_log;",
                "SELECT query FROM system.query_log WHERE event_date >= yesterday() AND type = 'QueryFinish'"
                f" AND query LIKE '%AS {marker} %' AND query NOT LIKE '%query_log%';",
            ]
        )
        got, stderr, code = run_shim(script)
        self.assertEqual(0, code, stderr)
        self.assertEqual(server_tsv("EXPLAIN SYNTAX SELECT 1") + f"SELECT 1 AS {marker} FORMAT Null;\n", got)

    def test_session_state(self):
        # SET and temporary tables last for the whole invocation, also after an error that a test
        # hint expects.
        script = "\n".join(
            [
                "SET max_threads = 3;",
                "SELECT throwIf(1); -- { serverError FUNCTION_THROW_IF_VALUE_IS_NON_ZERO }",
                "SELECT getSetting('max_threads');",
                "CREATE TEMPORARY TABLE t (x UInt8);",
                "INSERT INTO t VALUES (1), (2);",
                "INSERT INTO t FORMAT TSV",
                "3",
                "",
                "SELECT sum(x) FROM t;",
            ]
        )
        got, stderr, code = run_shim(script)
        self.assertEqual(0, code, stderr)
        self.assertEqual("3\n6\n", got)

    def test_command_line_settings(self):
        # The command-line settings go with each query. A SET of the same setting replaces the
        # value; the shim connects again, and the session (the temporary table) stays.
        script = "\n".join(
            [
                "CREATE TEMPORARY TABLE t (x UInt8);",
                "SELECT getSetting('log_comment'), getSetting('max_threads');",
                "SET log_comment = 'from-set';",
                "SELECT getSetting('log_comment'), getSetting('max_threads'), count() FROM t;",
            ]
        )
        got, stderr, code = run_shim(script, "--log_comment", "from-command-line", "--max_threads=2", "--max_block_size=7")
        self.assertEqual(0, code, stderr)
        self.assertEqual("from-command-line\t2\nfrom-set\t2\t0\n", got)

    def test_query_parameters(self):
        got, stderr, code = run_shim("SELECT {x:String}, {n:UInt8} + 1", "--param_x=a b;c", "--param_n=41")
        self.assertEqual(0, code, stderr)
        self.assertEqual("a b;c\t42\n", got)

    def test_use_database(self):
        db = "clickhouse_test_runner_live_use"
        script = f"DROP DATABASE IF EXISTS {db}; CREATE DATABASE {db}; USE {db}; SELECT currentDatabase(); DROP DATABASE {db};"
        got, stderr, code = run_shim(script)
        self.assertEqual(0, code, stderr)
        self.assertEqual(db + "\n", got)

    def test_errors(self):
        # An unexpected error stops the script, writes the error name to stderr, and sets the exit
        # code to the error code.
        got, stderr, code = run_shim("SELECT 1; SELECT * FROM no_such_table_xyz; SELECT 2;")
        self.assertEqual("1\n", got)
        self.assertEqual(60, code)
        self.assertIn("UNKNOWN_TABLE", stderr)

        # A query that succeeds although the hint expects an error.
        _, stderr, code = run_shim("SELECT 1; -- { serverError UNKNOWN_TABLE }")
        self.assertNotEqual(0, code)
        self.assertIn("expected serverError UNKNOWN_TABLE", stderr)

        # A different error than the hint expects.
        _, stderr, code = run_shim("SELECT * FROM no_such_table_xyz; -- { serverError SYNTAX_ERROR }")
        self.assertEqual(60, code, stderr)

        # A syntax error satisfies a clientError hint: the server finds it.
        got, stderr, code = run_shim("SELECT * FROM; -- { clientError SYNTAX_ERROR }\nSELECT 5;")
        self.assertEqual(0, code, stderr)
        self.assertEqual("5\n", got)

    def test_echo(self):
        got, stderr, code = run_shim("-- { echoOn }\nSELECT 1; -- one\nSELECT * FROM; -- { clientError SYNTAX_ERROR }\nSELECT 2\n")
        self.assertEqual(0, code, stderr)
        self.assertEqual("-- { echoOn }\nSELECT 1; -- one\n1\nSELECT 2\n\n2\n", got)


if __name__ == "__main__":
    unittest.main()
