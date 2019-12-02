import datetime
import decimal
import uuid

from testflows.core import TestFeature, TestScenario
from testflows.core import Requirements, Scenario, Given, When, Then, TE, run
from testflows.asserts import error
from requirements.QA_SRS003_ParameterizedQueries import *
from utils import Logs, PyODBCConnection

class Null(object):
    """NULL data type"""
    def __repr__(self):
        return 'NULL'

NULL = Null()

def check_datatype(connection, datatype, values, nullable=False, quote=False, repr=str, encoding="utf-8", expected=None):
    """Check support for a data type.
    """
    if expected is None:
        expected = dict()

    if nullable:
        datatype = f"Nullable({datatype})"
        values.append(NULL)

        if expected:
            expected["all"] = expected['all'].rsplit("]", 1)[0] + ", (None, )]"
            expected[NULL] = "[(None, )]"

    with Given("PyODBC connection"):
        with Given(f"parameters", description=f"""
            values {values}
            expected data {expected}
            """):

            with Given(f"table with a column of data type {datatype}"):
                connection.query("DROP TABLE IF EXISTS ps", fetch=False)
                connection.query(f"CREATE TABLE ps (v {datatype}) ENGINE = Memory", fetch=False)
                try:
                    connection.connection.setencoding(encoding=encoding)
                    for v in values:
                        with When(f"I insert value {repr(v)}", flags=TE):
                            # connection.query("INSERT INTO ps VALUES (?)", [v], fetch=False)
                            if quote:
                                connection.query(f"INSERT INTO ps VALUES ('{repr(v)}')", fetch=False)
                            else:
                                connection.query(f"INSERT INTO ps VALUES ({repr(v)})", fetch=False)

                    with When("I select all values", flags=TE):
                        rows = connection.query("SELECT * FROM ps ORDER BY v")
                        if expected.get("all") is not None:
                            with Then(f"the result is {expected.get('all')}", flags=TE):
                                assert repr(rows) == expected.get("all"), error("result did not match")

                    with When(f"I have values {repr(values)}"):
                        for v in values:
                            if v is NULL:
                                # comparing to NULL is not valid in SQL
                                continue
                            with When(f"I select value {repr(v)}", flags=TE):
                                rows = connection.query("SELECT * FROM ps WHERE v = ? ORDER BY v", [v])
                                if expected.get(v) is not None:
                                    with Then(f"the result is {repr(expected.get(v))}", flags=TE):
                                        assert repr(rows) == expected.get(v), error("result did not match")
                finally:
                    connection.connection.setencoding(encoding=connection.encoding)
                    connection.query("DROP TABLE ps", fetch=False)

@TestScenario
def sanity_check(connection):
    """Check connection to the database.
    """
    with Given("PyODBC connection"):
        with When("I do 'SELECT 1'"):
            rows = connection.query("SELECT 1")

        result = "[(1, )]"
        with Then(f"the result is {result}"):
            assert repr(rows) == result, error("result dit not match")

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Int8("1.0"))
def Int8(connection, nullable=False):
    """Verify support for Int8 data type."""
    check_datatype(connection, "Int8", [-128, 0, 127], expected={
            "all": "[(-128, ), (0, ), (127, )]",
            -128: "[(-128, )]",
            0: "[(0, )]",
            127: "[(127, )]"
        }, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Int16("1.0"))
def Int16(connection, nullable=False):
    """Verify support for Int16 data type."""
    check_datatype(connection, "Int16", [-32768, 0, 32767], expected={
            "all": "[(-32768, ), (0, ), (32767, )]",
            -32768: "[(-32768, )]",
            0: "[(0, )]",
            32767: "[(32767, )]"
        }, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Int32("1.0"))
def Int32(connection, nullable=False):
    """Verify support for Int32 data type."""
    check_datatype(connection, "Int32", [-2147483648, 0, 2147483647], expected={
            "all": "[(-2147483648, ), (0, ), (2147483647, )]",
            -2147483648: "[(-2147483648, )]",
            0: "[(0, )]",
            2147483647: "[(2147483647, )]"
        }, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Int64("1.0"))
def Int64(connection, nullable=False):
    """Verify support for Int64 data type."""
    check_datatype(connection, "Int64", [-9223372036854775808, 0, 9223372036854775807], expected={
            "all": "[(-9223372036854775808, ), (0, ), (9223372036854775807, )]",
            -9223372036854775808: "[(-9223372036854775808, )]",
            0: "[(0, )]",
            9223372036854775807: "[(9223372036854775807, )]"
        }, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_UInt8("1.0"))
def UInt8(connection, nullable=False):
    """Verify support for UInt8 data type."""
    check_datatype(connection, "UInt8", [0, 255], expected={
            "all": "[(0, ), (255, )]",
            0: "[(0, )]",
            255: "[(255, )]"
        }, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_UInt16("1.0"))
def UInt16(connection, nullable=False):
    """Verify support for UInt16 data type."""
    check_datatype(connection, "UInt16", [0, 65535], expected={
            "all": "[(0, ), (65535, )]",
            0: "[(0, )]",
            65535: "[(65535, )]"
        }, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_UInt32("1.0"))
def UInt32(connection, nullable=False):
    """Verify support for UInt32 data type."""
    check_datatype(connection, "UInt32", [0, 4294967295], expected={
            "all": "[(0, ), (4294967295, )]",
            0: "[(0, )]",
            4294967295: "[(4294967295, )]"
        }, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_UInt64("1.0"))
def UInt64(connection, nullable=False):
    """Verify support for UInt64 data type."""
    check_datatype(connection, "UInt64", [0, 18446744073709551615], expected={
            "all": "[(0, ), (18446744073709551615, )]",
            0: "[(0, )]",
            18446744073709551615: "[(18446744073709551615, )]"
        }, nullable=nullable)

@TestScenario
@Requirements(
    RQ_SRS_003_ParameterizedQueries_DataType_Select_Float32("1.0"),
    RQ_SRS_003_ParameterizedQueries_DataType_Select_Float32_Inf("1.0"),
    RQ_SRS_003_ParameterizedQueries_DataType_Select_Float32_NaN("1.0")
)
def Float32(connection, nullable=False):
    """Verify support for Float32 data type."""
    check_datatype(connection, "Float32", [-1, 0, float("inf"), float("-inf"), float("nan"), 13.26], expected={
            "all": "[(-inf, ), (-1.0, ), (0.0, ), (13.26, ), (inf, ), (nan, )]",
            0: "[(0.0, )]",
            -1: "[(-1.0, )]",
            13.26: "[(13.26, )]",
            float("inf"): "[(inf, )]",
            float("-inf"): "[(-inf, )]",
            float("nan"): "[(nan, )]"
        }, nullable=nullable)

@TestScenario
@Requirements(
    RQ_SRS_003_ParameterizedQueries_DataType_Select_Float64("1.0"),
    RQ_SRS_003_ParameterizedQueries_DataType_Select_Float64_Inf("1.0"),
    RQ_SRS_003_ParameterizedQueries_DataType_Select_Float64_NaN("1.0")
)
def Float64(connection, nullable=False):
    """Verify support for Float64 data type."""
    check_datatype(connection, "Float64", [-1, 0, float("inf"), 13.26, float("-inf"), float("nan")], expected={
            "all": "[(-inf, ), (-1.0, ), (0.0, ), (13.26, ), (inf, ), (nan, )]",
            0: "[(0.0, )]",
            -1: "[(-1.0, )]",
            13.26: "[(13.26, )]",
            float("inf"): "[(inf, )]",
            float("-inf"): "[(-inf, )]",
            float("nan"): "[(nan, )]"
        }, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Decimal32("1.0"))
def Decimal32(connection, nullable=False):
    """Verify support for Decimal32 data type."""
    expected = {
        "all": "[((Decimal('-99999.9999'), ), (Decimal('10.1234'), ), (Decimal('99999.9999'), )]",
        decimal.Decimal('-99999.9999'): "[((Decimal('-99999.9999'), )]",
        decimal.Decimal('10.1234'): "[(Decimal('10.1234'), )]",
        decimal.Decimal('99999.9999'): "[(Decimal('99999.9999'), ]"
    }

    check_datatype(connection, "Decimal32(4)", [
            decimal.Decimal('-99999.9999'),
            decimal.Decimal('10.1234'),
            decimal.Decimal('99999.9999')
        ], expected=expected, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Decimal64("1.0"))
def Decimal64(connection, nullable=False):
    """Verify support for Decimal64 data type."""
    expected = {
        "all": "[((Decimal('-99999999999999.9999'), ), (Decimal('10.1234'), ), (Decimal('99999999999999.9999'), )]",
        decimal.Decimal('-99999999999999.9999'): "[((Decimal('-99999999999999.9999'), )]",
        decimal.Decimal('10.1234'): "[(Decimal('10.1234'), )]",
        decimal.Decimal('99999999999999.9999'): "[(Decimal('99999999999999.9999'), ]"
    }

    check_datatype(connection, "Decimal64(4)", [
            decimal.Decimal('-99999999999999.9999'),
            decimal.Decimal('10.1234'),
            decimal.Decimal('99999999999999.9999')
        ], expected=expected, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Decimal128("1.0"))
def Decimal128(connection, nullable=False):
    """Verfiy support for Decimal128 data type."""
    expected = {
        "all": "[((Decimal('-9999999999999999999999999999999999.9999'), ), (Decimal('10.1234'), ), (Decimal('9999999999999999999999999999999999.9999'), )]",
        decimal.Decimal('-9999999999999999999999999999999999.9999'): "[((Decimal('-9999999999999999999999999999999999.9999'), )]",
        decimal.Decimal('10.1234'): "[(Decimal('10.1234'), )]",
        decimal.Decimal('9999999999999999999999999999999999.9999'): "[(Decimal('9999999999999999999999999999999999.9999'), ]"
    }

    check_datatype(connection, "Decimal128(4)", [
            decimal.Decimal('-9999999999999999999999999999999999.9999'),
            decimal.Decimal('10.1234'),
            decimal.Decimal('9999999999999999999999999999999999.9999')
        ], expected=expected, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_String("1.0"))
def String(connection, nullable=False):
    """Verify support for String data type."""

    with Scenario("empty",
            description="Check empty string.",
            flags=TE,
            requirements=[RQ_SRS_003_ParameterizedQueries_DataType_Select_String_Empty("1.0")]):

        with Scenario("utf-8", flags=TE, description="UTF-8 encoding"):
            values = ["", b''.decode("utf-8")]
            expected = {
                "all": f"[('{values[0]}', ), ('{values[1]}', )]",
                values[0]: f"[('{values[0]}', ), ('{values[1]}', )]",
                values[1]: f"[('{values[0]}', ), ('{values[1]}', )]"
            }
            check_datatype(connection, "String", values=values, expected=expected,
                encoding="utf-8", quote=True, nullable=nullable)

        with Scenario("ascii", flags=TE, description="ASCII encoding."):
            values = ["", b''.decode("ascii")]
            expected = {
                "all": f"[('{values[0]}', ), ('{values[1]}', )]",
                values[0]: f"[('{values[0]}', ), ('{values[1]}', )]",
                values[1]: f"[('{values[0]}', ), ('{values[1]}', )]"
            }
            check_datatype(connection, "String", values=values, expected=expected,
                encoding="ascii", quote=True, nullable=nullable)

    with Scenario("utf8",
            flags=TE,
            requirements=[RQ_SRS_003_ParameterizedQueries_DataType_Select_String_UTF8("1.0")],
            description="Check UTF-8 encoding."
        ):
        values = [
                "hello",
                (b'\xe5\x8d\xb0\xe5\x88\xb7\xe5\x8e\x82\xe6\x8b\xbf\xe8\xb5\xb7'
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
            ]
        expected = {
            "all": f"[('{values[0]}', ), ('{values[1]}', )]",
            values[0]: f"[('{values[0]}', )]",
            values[1]: f"[('{values[1]}', )]"
        }
        check_datatype(connection, "String", values=values, expected=expected,
            encoding="utf-8", quote=True, nullable=nullable)

    with Scenario("ascii",
            flags=TE,
            requirements=[RQ_SRS_003_ParameterizedQueries_DataType_Select_String_ASCII("1.0")],
            description="Check ASCII encoding."
        ):
        values = [
            "hello",
            r' !"#$%%&()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}~'
        ]
        expected = {
            "all": f"[('{values[1]}', ), ('{values[0]}', )]",
            values[0]: f"[('{values[0]}', )]",
            values[1]: f"[('{values[1]}', )]"
        }
        check_datatype(connection, "String", values=values, expected=expected,
            encoding="ascii", quote=True, nullable=nullable)

    with Scenario("binary",
            flags=TE,
            requirements=[RQ_SRS_003_ParameterizedQueries_DataType_Select_String_Binary("1.0")],
            description="Check binary data."
        ):
        values = [
            "\x00\x01\x02\0x03\x00\x00\xFF"
        ]
        expected = {
            "all": f"[('{values[0]}', )]",
            values[0]: f"[('{values[0]}', )]",
        }
        check_datatype(connection, "String", values=values, expected=expected, encoding="ascii", quote=False, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_FixedString("1.0"))
def FixedString(connection, nullable=False):
    """Verify support for FixedString data type."""
    with Scenario("utf8", flags=TE, description="UTF-8 encoding"):
        values = [
            "",
            "hello",
            (b'\xe5\x8d\xb0\xe5\x88\xb7\xe5\x8e\x82\xe6\x8b\xbf\xe8\xb5\xb7').decode("utf-8")
        ]
        expected = {
            "all": f"[('\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00', ), ('hello\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00', ), ('{values[2]}\\x00', )]",
            values[0]: "[('\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00', )]",
            values[1]: "[('hello\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00', )]",
            values[2]: f"[('{values[2]}\\x00', )]"
        }
        check_datatype(connection, "FixedString(16)", values=values, expected=expected,
            encoding="utf-8", quote=True, nullable=nullable)

    with Scenario("ascii", flags=TE, description="ASCII encoding."):
        values = [
            "",
            "hello",
            "ABCDEFGHIJKLMN"
        ]
        expected = {
            "all": "[('\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00', ), ('ABCDEFGHIJKLMN\\x00\\x00', ), ('hello\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00', )]",
            values[0]: "[('\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00', )]",
            values[1]: "[('hello\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00', )]",
            values[2]: "[('ABCDEFGHIJKLMN\\x00\\x00', )]"
        }
        check_datatype(connection, "FixedString(16)", values=values, expected=expected,
            encoding="ascii", quote=True, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Date("1.0"))
def Date(connection, nullable=False):
    """Verify support for Date date type."""
    values = [
        datetime.date(1970, 3, 3),
        datetime.date(2000, 12, 31),
        datetime.date(2024, 5, 5)
    ]
    expected = {
        "all": "[(datetime.date(1970, 3, 3), ), (datetime.date(2000, 12, 31), ), (datetime.date(2024, 5, 5), )]",
        values[0]: "[(datetime.date(1970, 3, 3), )]",
        values[1]: "[(datetime.date(2000, 12, 31), )]",
        values[2]: "[(datetime.date(2024, 5, 5), )]"
    }
    check_datatype(connection, "Date", values=values, expected=expected, quote=True, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_DateTime("1.0"))
def DateTime(connection, nullable=False):
    """Verify support for DateTime data type."""
    values = [
        datetime.datetime(1970, 3, 3, 0, 0, 0),
        datetime.datetime(2000, 12, 31, 23, 59, 59),
        datetime.datetime(2024, 5, 5, 13, 31, 32)
    ]
    expected = {
        "all": "[(datetime.datetime(1970, 3, 3, 0, 0), ), (datetime.datetime(2000, 12, 31, 23, 59, 59), ), (datetime.datetime(2024, 5, 5, 13, 31, 32), )]",
        values[0]: "[(datetime.datetime(1970, 3, 3, 0, 0), )]",
        values[1]: "[(datetime.datetime(2000, 12, 31, 23, 59, 59), )]",
        values[2]: "[(datetime.datetime(2024, 5, 5, 13, 31, 32), )]"
    }
    check_datatype(connection, "DateTime", values=values, expected=expected, quote=True, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Enum("1.0"))
def Enum(connection, nullable=False):
    """Verify support for Enum data type."""
    with Scenario("utf8", flags=TE, description="UTF-8 encoding"):
        key0 = b'\xe5\x8d\xb0'.decode('utf-8')
        key1 = b'\xe5\x88\xb7'.decode('utf-8')
        check_datatype(connection, f"Enum('{key0}' = 1, '{key1}' = 2)", [key0, key1], expected={
                "all": f"[('{key0}', ), ('{key1}', )]",
                key0: f"[('{key0}', )]",
                key1: f"[('{key1}', )]"
            }, encoding="utf-8", quote=True, nullable=nullable)

    with Scenario("ascii", flags=TE, description="ASCII encoding"):
        check_datatype(connection, "Enum('hello' = 1, 'world' = 2)", ["hello", "world"], expected={
                "all": "[('hello', ), ('world', )]",
                "hello": "[('hello', )]",
                "world": "[('world', )]"
            }, encoding="ascii", quote=True, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_UUID("1.0"))
def UUID(connection, nullable=False):
    """Verify support for UUID data type."""
    uuid0 = "417ddc5d-e556-4d27-95dd-a34d84e46a50"
    uuid1 = "417ddc5d-e556-4d27-95dd-a34d84e46a51"
    uuid2 = uuid.UUID('1dc3c592-f333-11e9-bedd-2477034de0ec')

    values = [uuid0, uuid1, uuid2]
    expected = {
        "all": f"[('{uuid0}', ), ('{uuid1}', ), ('{uuid2}', )]",
        uuid0: f"[('{uuid0}', )]",
        uuid1: f"[('{uuid1}', )]",
        uuid2: f"[('{uuid2}', )]"
    }
    check_datatype(connection, "UUID", values=values, expected=expected, quote=True, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_IPv4("1.0"))
def IPv4(connection, nullable=False):
    """Verify support for IPv4 data type."""
    ipv40 = "116.106.34.242"
    ipv41 = "116.253.40.133"

    values = [ipv40, ipv41]
    expected = {
        "all": f"[('{ipv40}', ), ('{ipv41}', )]",
        ipv40: f"[('{ipv40}', )]",
        ipv41: f"[('{ipv41}', )]"
    }
    check_datatype(connection, "IPv4", values=values, expected=expected, quote=True, nullable=nullable)

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_IPv6("1.0"))
def IPv6(connection, nullable=False):
    """Verify support for IPv6 data type."""
    ipv60 = "2001:44c8:129:2632:33:0:252:2"
    ipv61 = "2a02:e980:1e::1"

    values = [ipv60, ipv61]
    expected = {
        "all": f"[('{ipv60}', ), ('{ipv61}', )]",
        ipv60: f"[('{ipv60}', )]",
        ipv61: f"[('{ipv61}', )]"
    }
    check_datatype(connection, "IPv6", values=values, expected=expected, quote=True, nullable=nullable)

@TestFeature
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Nullable("1.0"))
def nullable():
    """Check support for Nullable data types."""
    datatypes(args={"nullable": True})

@TestFeature
@Requirements(RQ_SRS_003_ParameterizedQueries_DataTypes("1.0"))
def datatypes(nullable=False):
    """Check clickhouse-odbc driver support for parameterized
    queries with various data types using pyodbc connector.
    """
    with Logs() as logs, PyODBCConnection(logs=logs) as connection:
        args = {"connection": connection, "nullable": nullable}

        run("Sanity check", sanity_check, args={"connection": connection})
        run("Check support for Int8", Int8, args=args, flags=TE)
        run("Check support for Int16", Int16, args=args, flags=TE)
        run("Check support for Int32", Int32, args=args, flags=TE)
        run("Check support for Int64", Int64, args=args, flags=TE)
        run("Check support for UInt8", UInt8, args=args, flags=TE)
        run("Check support for UInt16", UInt16, args=args, flags=TE)
        run("Check support for UInt32", UInt32, args=args, flags=TE)
        run("Check support for UInt64", UInt64, args=args, flags=TE)
        run("Check support for Float32", Float32, args=args, flags=TE)
        run("Check support for Float64", Float64, args=args, flags=TE)
        run("Check support for Decimal32", Decimal32, args=args, flags=TE)
        run("Check support for Decimal64", Decimal64, args=args, flags=TE)
        run("Check support for Decimal128", Decimal128, args=args, flags=TE)
        run("Check support for String", String, args=args, flags=TE)
        run("Check support for FixedString", FixedString, args=args, flags=TE)
        run("Check support for Date", Date, args=args, flags=TE)
        run("Check support for DateTime", DateTime, args=args, flags=TE)
        run("Check support for Enum", Enum, args=args, flags=TE)
        run("Check support for UUID", UUID, args=args, flags=TE)
        run("Check support for IPv4", IPv4, args=args, flags=TE)
        run("Check support for IPv6", IPv6, args=args, flags=TE)
