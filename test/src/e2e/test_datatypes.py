import datetime
import decimal
import math
import uuid

import pytest

from util import pyodbc_connection, create_table, rows_as_values


# FIXME: None is converted to an empty string (probably by PyODBC itself?)
#  After the fix, Nullable test cases should be re-added
#  Sample error: Attempt to read after eof: while converting '' to UInt8. (ATTEMPT_TO_READ_AFTER_EOF)
#
# TODO:
#  Bool
#  (U)Int128
#  (U)Int256
#  Decimal256
#  DateTime64
#  Array
#  Tuple
#  Map
#  LowCardinality
class TestDataTypes:
    def test_bool(self):
        """ Bool are currently converted to a string.
        This should be fixed in the future. The test demonstrates the problem.
        """
        table_name = "odbc_test_data_types_bool"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "b Bool")):
            values = ['true', 'false'] # strings, but should be `[True, False]`
            conn.insert(table_name, "(true), (false)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE b = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "b"
                assert rows[0].cursor_description[0][1] == str # should be `bool`

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 2
            assert rows_as_values(rows) == values

    def test_int8(self):
        table_name = "odbc_test_data_types_int8"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "i Int8")):
            values = [-128, 0, 127]
            conn.insert(table_name, "(-128), (0), (127)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE i = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "i"
                assert rows[0].cursor_description[0][1] == int

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_int16(self):
        table_name = "odbc_test_data_types_int16"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "i Int16")):
            values = [-32768, 0, 32767]
            conn.insert(table_name, "(-32768), (0), (32767)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE i = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "i"
                assert rows[0].cursor_description[0][1] == int

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_int32(self):
        table_name = "odbc_test_data_types_int32"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "i Int32")):
            values = [-2147483648, 0, 2147483647]
            conn.insert(table_name, "(-2147483648), (0), (2147483647)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE i = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "i"
                assert rows[0].cursor_description[0][1] == int

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_int64(self):
        table_name = "odbc_test_data_types_int64"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "i Int64")):
            values = [-9223372036854775808, 0, 9223372036854775807]
            conn.insert(table_name, "(-9223372036854775808), (0), (9223372036854775807)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE i = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "i"
                assert rows[0].cursor_description[0][1] == int

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_uint8(self):
        table_name = "odbc_test_data_types_uint8"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "i UInt8")):
            values = [0, 255]
            conn.insert(table_name, "(0), (255)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE i = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "i"
                assert rows[0].cursor_description[0][1] == int

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 2
            assert rows_as_values(rows) == values

    def test_uint16(self):
        table_name = "odbc_test_data_types_uint16"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "i UInt16")):
            values = [0, 65535]
            conn.insert(table_name, "(0), (65535)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE i = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "i"
                assert rows[0].cursor_description[0][1] == int

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 2
            assert rows_as_values(rows) == values

    def test_uint32(self):
        table_name = "odbc_test_data_types_uint32"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "i UInt32")):
            values = [0, 4294967295]
            conn.insert(table_name, "(0), (4294967295)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE i = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "i"
                assert rows[0].cursor_description[0][1] == int

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 2
            assert rows_as_values(rows) == values

    def test_uint64(self):
        table_name = "odbc_test_data_types_uint64"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "i UInt64")):
            values = [0, 18446744073709551615]
            conn.insert(table_name, "(0), (18446744073709551615)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE i = ?",
                                  [str(value)])  # UInt64 max value overflows, bind as a string
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "i"
                assert rows[0].cursor_description[0][1] == int

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 2
            assert rows_as_values(rows) == values

    @pytest.mark.parametrize("ch_type", ["Float32", "Float64"])
    def test_float(self, ch_type):
        table_name = f"odbc_test_data_types_{ch_type.lower()}"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, f"f {ch_type}")):
            values = [-1.0, 0.0, float("inf"), float("-inf"), 13.26]  # NaN handled separately
            conn.insert(table_name, "(-1), (0), (inf), (-inf), (nan), (13.26)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE f = ?",
                                  [str(value)])  # Avoid float precision issues
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "f"
                assert rows[0].cursor_description[0][1] == float

            rows = conn.query(f"SELECT * FROM {table_name} WHERE isNaN(f)")
            assert len(rows) == 1
            assert math.isnan(rows_as_values(rows)[0])

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 6
            result_values = rows_as_values(rows)
            assert result_values[0] == -1.0
            assert result_values[1] == 0.0
            assert result_values[2] == float("inf")
            assert result_values[3] == float("-inf")
            assert math.isnan(result_values[4])
            assert result_values[5] == 13.26

    def test_decimal32(self):
        table_name = "odbc_test_data_types_decimal32"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "d Decimal32(4)")):
            values = [decimal.Decimal("-99999.9999"),
                      decimal.Decimal("10.1234"),
                      decimal.Decimal("99999.9999")]
            conn.insert(table_name, "(-99999.9999), (10.1234), (99999.9999)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE d = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "d"
                assert rows[0].cursor_description[0][1] == decimal.Decimal

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_decimal64(self):
        table_name = "odbc_test_data_types_decimal64"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "d Decimal64(4)")):
            values = [decimal.Decimal("-99999999999999.9999"),
                      decimal.Decimal("10.1234"),
                      decimal.Decimal("99999999999999.9999")]
            conn.insert(table_name, "(-99999999999999.9999), (10.1234), (99999999999999.9999)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE d = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "d"
                assert rows[0].cursor_description[0][1] == decimal.Decimal

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_decimal128(self):
        table_name = "odbc_test_data_types_decimal128"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "d Decimal128(4)")):
            values = [decimal.Decimal("-9999999999999999999999999999999999.9999"),
                      decimal.Decimal("10.1234"),
                      decimal.Decimal("9999999999999999999999999999999999.9999")]
            conn.insert(table_name, "(-9999999999999999999999999999999999.9999), (10.1234), (9999999999999999999999999999999999.9999)")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE d = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "d"
                assert rows[0].cursor_description[0][1] == decimal.Decimal

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_string(self):
        table_name = "odbc_test_data_types_string"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "s String")):
            values = ["", "hello", "world", "hello, world"]
            conn.insert(table_name, "(''), ('hello'), ('world'), ('hello, world')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE s = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "s"
                assert rows[0].cursor_description[0][1] == str

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 4
            assert rows_as_values(rows) == values

    def test_string_utf8_and_binary(self):
        table_name = "odbc_test_data_types_string_utf8"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "s String")):
            utf8_string1 = "¶"
            utf8_string2 = (b'\xe5\x8d\xb0\xe5\x88\xb7\xe5\x8e\x82\xe6\x8b\xbf\xe8\xb5\xb7'
                            b'\xe4\xb8\x80\xe4\xb8\xaa\xe6\xa0\xb7\xe6\x9d\xbf\xe9\x97\xb4'
                            b'\xef\xbc\x8c\xe7\x84\xb6\xe5\x90\x8e\xe5\xb0\x86\xe5\x85\xb6'
                            b'\xe6\x89\x93\xe6\x8b\xbc\xe6\x88\x90\xe6\xa0\xb7\xe6\x9c\xac'
                            b'\xe3\x80\x82 \xe5\xae\x83\xe4\xb8\x8d\xe4\xbb\x85\xe7\x94\x9f'
                            b'\xe5\xad\x98\xe4\xba\x86\xe4\xba\x94\xe4\xb8\xaa\xe4\xb8\x96'
                            b'\xe7\xba\xaa\xef\xbc\x8c\xe8\x80\x8c\xe4\xb8\x94\xe5\x9c\xa8'
                            b'\xe7\x94\xb5\xe5\xad\x90\xe6\x8e\x92\xe7\x89\x88\xe6\x96\xb9'
                            b'\xe9\x9d\xa2\xe4\xb9\x9f\xe5\x8f\x96\xe5\xbe\x97\xe4\xba\x86'
                            b'\xe9\xa3\x9e\xe8\xb7\x83\xef\xbc\x8c\xe4\xbd\x86\xe5\x9f\xba'
                            b'\xe6\x9c\xac\xe4\xb8\x8a\xe6\xb2\xa1\xe6\x9c\x89\xe6\x94\xb9'
                            b'\xe5\x8f\x98\xe3\x80\x82 \xe5\xae\x83\xe5\x9c\xa81960\xe5\xb9'
                            b'\xb4\xe4\xbb\xa3\xe9\x9a\x8f\xe7\x9d\x80Letraset\xe5\xba\x8a'
                            b'\xe5\x8d\x95\xe7\x9a\x84\xe5\x8f\x91\xe5\xb8\x83\xe8\x80\x8c'
                            b'\xe6\x99\xae\xe5\x8f\x8a\xef\xbc\x8c\xe5\x85\xb6\xe4\xb8\xad'
                            b'\xe5\x8c\x85\xe5\x90\xabLerem Ipsum\xe6\xae\xb5\xe8\x90\xbd'
                            b'\xe7\xad\x89').decode("utf-8")
            binary_string = "\x00\x01\x02\0x03\x00\x00\xFF"
            values = [
                "hello",
                utf8_string1,
                utf8_string2,
                binary_string
            ]
            insert_values = ','.join(list(map(lambda x: f"('{x}')", values)))
            conn.insert(table_name, insert_values)

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE s = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "s"
                assert rows[0].cursor_description[0][1] == str

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 4
            assert rows_as_values(rows) == values

    def test_fixed_string(self):
        table_name = "odbc_test_data_types_fixed_string"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "s FixedString(6)")):
            values = ["hello\x00", "world\x00", "hellow"]
            conn.insert(table_name, "('hello'), ('world'), ('hellow')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE s = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "s"
                assert rows[0].cursor_description[0][1] == str

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_fixed_string_utf8(self):
        table_name = "odbc_test_data_types_fixed_string_utf8"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "s FixedString(6)")):
            values = ["h¶\x00\x00\x00", "w¶¶\x00", "hellow"]  # ¶ = 2 bytes
            conn.insert(table_name, "('h¶'), ('w¶¶'), ('hellow')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE s = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "s"
                assert rows[0].cursor_description[0][1] == str

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_date(self):
        table_name = "odbc_test_data_types_date"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "d Date")):
            values = [
                datetime.date(1970, 1, 1),
                datetime.date(2000, 12, 31),
                datetime.date(2020, 1, 1),
                datetime.date(2149, 6, 6)]
            conn.insert(table_name, "('1970-01-01'), ('2000-12-31'), ('2020-01-01'), ('2149-06-06')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE d = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "d"
                assert rows[0].cursor_description[0][1] == datetime.date

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 4
            assert rows_as_values(rows) == values

    def test_datetime(self):
        table_name = "odbc_test_data_types_datetime"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "dt DateTime")):
            values = [
                # FIXME: 0 unix time assertion will fail if the server timezone is not UTC even with SETTINGS session_timezone='UTC'
                #  Could be a potential bug
                # datetime.datetime(1970, 1, 1, 0, 0, 0),
                datetime.datetime(2000, 12, 31, 23, 59, 59),
                datetime.datetime(2020, 1, 1, 1, 1, 1),
                datetime.datetime(2106, 2, 7, 6, 28, 15)]
            conn.insert(table_name, "('2000-12-31 23:59:59'), ('2020-01-01 01:01:01'), ('2106-02-07 06:28:15')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE dt = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "dt"
                assert rows[0].cursor_description[0][1] == datetime.datetime

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_datetime64(self):
        table_name = "odbc_test_data_types_datetime_insert"
        # Python's datetime only supports microseconds precision
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "dt DateTime64(6)")):
            values = [
                datetime.datetime(2000, 12, 31, 23, 59, 59, 999999),
                datetime.datetime(2020, 1, 1, 1, 1, 1, 1),
                datetime.datetime(2106, 2, 7, 6, 28, 15, 0)]
            conn.insert(table_name, "('2000-12-31 23:59:59.999999'), ('2020-01-01 01:01:01.000001'), ('2106-02-07 06:28:15')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE dt = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "dt"
                assert rows[0].cursor_description[0][1] == datetime.datetime

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_datetime_insert(self):
        """Test inserting values into DateTime and DateTime64 columns

        Python's `datetime` is bound to the DateTime64 type in ClickHouse. This is also true
        for other client ODBC libraries—when they bind SQL_TYPE_TIMESTAMP, DateTime64 is
        used under the hood. We need to ensure that we can insert into both DateTime64 and
        DateTime using parameter binding.
        """
        table_name = "odbc_test_data_types_datetime_insert"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "dt DateTime, dt64 DateTime64(6)")):

            dt = datetime.datetime(2025, 4, 14, 11, 11, 5, 0)
            dt64 = datetime.datetime(2025, 4, 14, 11, 11, 5, 123456)
            conn.query(f"INSERT INTO {table_name} VALUES (?, ?)", [dt, dt64], fetch=False);

            rows = conn.query(f"SELECT dt, dt64 FROM {table_name}");
            assert len(rows) == 1
            assert list(rows[0]) == [dt, dt64]

    def test_enum8(self):
        table_name = "odbc_test_data_types_enum8"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "e Enum8('hello' = -128, '¶' = 42, 'world' = 127)")):
            values = ["hello", "¶", "world"]
            conn.insert(table_name, "('hello'), ('¶'), ('world')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE e = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "e"
                assert rows[0].cursor_description[0][1] == str

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_enum16(self):
        table_name = "odbc_test_data_types_enum16"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "e Enum16('hello' = -32768, '¶' = 42, 'world' = 32767)")):
            values = ["hello", "¶", "world"]
            conn.insert(table_name, "('hello'), ('¶'), ('world')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE e = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "e"
                assert rows[0].cursor_description[0][1] == str

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == values

    def test_uuid(self):
        table_name = "odbc_test_data_types_uuid"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "u UUID")):
            uuid0 = "417ddc5d-e556-4d27-95dd-a34d84e46a50"
            uuid1 = "417ddc5d-e556-4d27-95dd-a34d84e46a51"
            uuid2 = uuid.UUID('1dc3c592-f333-11e9-bedd-2477034de0ec')
            values = [uuid0, uuid1, uuid2]
            conn.insert(table_name, f"('{uuid0}'), ('{uuid1}'), ('{(str(uuid2))}')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE u = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [str(value)]
                assert rows[0].cursor_description[0][0] == "u"
                assert rows[0].cursor_description[0][1] == str

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 3
            assert rows_as_values(rows) == list(map(str, values))

    def test_ipv4(self):
        table_name = "odbc_test_data_types_ipv4"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "i IPv4")):
            ipv40 = "116.106.34.242"
            ipv41 = "116.253.40.133"
            values = [ipv40, ipv41]
            conn.insert(table_name, f"('{ipv40}'), ('{ipv41}')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE i = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "i"
                assert rows[0].cursor_description[0][1] == str

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 2
            assert rows_as_values(rows) == values

    def test_ipv6(self):
        table_name = "odbc_test_data_types_ipv6"
        with (pyodbc_connection() as conn,
              create_table(conn, table_name, "i IPv6")):
            ipv60 = "2001:db8:85a3::8a2e:370:7334"
            ipv61 = "2001:db8:85a3::8a2e:370:7335"
            values = [ipv60, ipv61]
            conn.insert(table_name, f"('{ipv60}'), ('{ipv61}')")

            for value in values:
                rows = conn.query(f"SELECT * FROM {table_name} WHERE i = ?", [value])
                assert len(rows) == 1
                assert rows_as_values(rows) == [value]
                assert rows[0].cursor_description[0][0] == "i"
                assert rows[0].cursor_description[0][1] == str

            rows = conn.query(f"SELECT * FROM {table_name}")
            assert len(rows) == 2
            assert rows_as_values(rows) == values
