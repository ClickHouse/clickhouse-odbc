import datetime
from decimal import Decimal

import pytest

import pyodbc

from util import pyodbc_connection, create_table

TABLE_NAME = "test_sanity_simple_data_types"
TABLE_SCHEMA = "i UInt8, ni Nullable(UInt8), s String, d Date, dt DateTime, f Float32, dc Decimal32(3), fs FixedString(8)"

VALUES_ROW1 = "(1, NULL, 'Hello, world', '2005-05-05', '2005-05-05 05:05:05', 1.5, 10.123, 'fstring0')"
PYODBC_ROW1 = [1, None, 'Hello, world', datetime.date(2005, 5, 5), datetime.datetime(2005, 5, 5, 5, 5, 5), 1.5,
               Decimal('10.123'), 'fstring0']

VALUES_ROW2 = "(2, NULL, 'test', '2019-05-25', '2019-05-25 15:00:00', 1.433, 11.124, 'fstring1')"
PYODBC_ROW2 = [2, None, 'test', datetime.date(2019, 5, 25), datetime.datetime(2019, 5, 25, 15, 0), 1.433, Decimal('11.124'), 'fstring1']


class TestSanity:
    @pytest.fixture(scope='class')
    def conn(self):
        with (pyodbc_connection() as conn,
              create_table(conn, TABLE_NAME, TABLE_SCHEMA)):
            conn.insert(TABLE_NAME, VALUES_ROW1)
            conn.insert(TABLE_NAME, VALUES_ROW2)
            yield conn

    def test_simple_query(self):
        with pyodbc_connection() as conn:
            def query(_query, *args, **kwargs):
                return conn.query(_query, *args, **kwargs)

            result = query("SELECT 1")
            assert len(result) == 1
            assert result[0].cursor_description[0][0] == "1"
            assert result[0].cursor_description[0][1] == int

    def test_uint8_param(self, conn):
        result = conn.query(f"SELECT * FROM {TABLE_NAME} WHERE i = ? ORDER BY i, s, d",
                            [1])
        assert len(result) == 1
        assert list(result[0]) == PYODBC_ROW1

    # FIXME: None is converted to an empty string (probably by PyODBC itself?)
    #  Rendered query: SELECT * FROM test_sanity_simple_data_types WHERE ni = _CAST(NULL, 'Nullable(String)') ORDER BY i ASC, s ASC, d ASC
    #  Attempt to read after eof: while converting '' to UInt8. (ATTEMPT_TO_READ_AFTER_EOF)
    # def test_nullable_uint8_param(self, conn):
    #     result = conn.query(f"SELECT * FROM {TABLE_NAME} WHERE ni = NULL ORDER BY i, s, d",
    #                         [None])
    #     assert len(result) == 1
    #     assert list(result[0]) == PYODBC_ROW1

    def test_string_param(self, conn):
        result = conn.query(f"SELECT * FROM {TABLE_NAME} WHERE s = ? ORDER BY i, s, d",
                            ["Hello, world"])
        assert len(result) == 1
        assert list(result[0]) == PYODBC_ROW1

    def test_date_param(self, conn):
        result = conn.query(f"SELECT * FROM {TABLE_NAME} WHERE d = ? ORDER BY i, s, d",
                            [datetime.date(2005, 5, 5)])
        assert len(result) == 1
        assert list(result[0]) == PYODBC_ROW1

    def test_datetime_param(self, conn):
        result = conn.query(f"SELECT * FROM {TABLE_NAME} WHERE dt = ? ORDER BY i, s, d",
                            [datetime.datetime(2005, 5, 5, 5, 5, 5)])
        assert len(result) == 1
        assert list(result[0]) == PYODBC_ROW1

    def test_float32_param(self, conn):
        result = conn.query(f"SELECT * FROM {TABLE_NAME} WHERE f = ? ORDER BY i, s, d",
                            [1.5])
        assert len(result) == 1
        assert list(result[0]) == PYODBC_ROW1

    def test_decimal32_param(self, conn):
        result = conn.query(f"SELECT * FROM {TABLE_NAME} WHERE dc = ? ORDER BY i, s, d",
                            [Decimal('10.123')])
        assert len(result) == 1
        assert list(result[0]) == PYODBC_ROW1

    def test_fixed_string_param(self, conn):
        result = conn.query(f"SELECT * FROM {TABLE_NAME} WHERE fs = ? ORDER BY i, s, d",
                            ["fstring0"])
        assert len(result) == 1
        assert list(result[0]) == PYODBC_ROW1

    def test_uint8_and_string_params(self, conn):
        result = conn.query(f"SELECT * FROM {TABLE_NAME} WHERE i = ? and s = ? ORDER BY i, s, d",
                            [2, "test"])
        assert len(result) == 1
        assert list(result[0]) == PYODBC_ROW2

    def test_uint8_string_and_date_params(self, conn):
        result = conn.query(f"SELECT * FROM {TABLE_NAME} WHERE i = ? and s = ? and d = ? ORDER BY i, s, d",
                            [2, "test", datetime.date(2019, 5, 25)])
        assert len(result) == 1
        assert list(result[0]) == PYODBC_ROW2

    def test_dbms_version(self, conn):
        param_version = conn.connection.getinfo(pyodbc.SQL_DBMS_VER)

        result = conn.query("SELECT version()")
        query_version, = list(result[0])

        assert param_version == query_version
