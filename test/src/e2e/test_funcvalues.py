import datetime

from util import pyodbc_connection


class TestFuncValues:
    def test_is_null_false(self):
        with pyodbc_connection() as conn:
            values = [
                "hello",
                b'\xe5\x8d\xb0'.decode('utf-8'),
                -1,
                0,
                255,
                1.0,
                0.0,
                -1.0,
                datetime.date(2000, 12, 31),
                datetime.datetime(2000, 12, 31, 23, 59, 59),
            ]
            for value in values:
                rows = conn.query("SELECT isNull(?)", [value])
                assert repr(rows) == "[(0,)]", f"result did not match for value {value}"

    def test_is_null_true(self):
        with pyodbc_connection() as conn:
            rows = conn.query("SELECT isNull(?)", [None])
            assert repr(rows) == "[(1,)]"

    def test_array_reduce_null(self):
        with pyodbc_connection() as conn:
            rows = conn.query("SELECT arrayReduce('count', [?, ?])", [None, None])
            assert repr(rows) == "[(0,)]"

    # FIXME:
    #  Fails with a NO_COMMON_TYPE error
    #  Rendered query:
    #  SELECT arrayReduce('count', [1, _CAST(NULL, 'LowCardinality(Nullable(String))'), _CAST(NULL, 'LowCardinality(Nullable(String))')])
    # def test_array_reduce_not_null(self):
    #     with pyodbc_connection() as conn:
    #         rows = conn.query("SELECT arrayReduce('count', [1, ?, ?])", [None, None])
    #         assert repr(rows) == "[(1,)]"
