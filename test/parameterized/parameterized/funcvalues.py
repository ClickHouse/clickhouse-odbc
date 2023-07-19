import datetime

from testflows.core import TestFeature, TestScenario
from testflows.core import Scenario, Given, When, Then
from testflows.core import Requirements, Name, TE
from testflows.asserts import error
from utils import Logs, PyODBCConnection

from requirements.QA_SRS003_ParameterizedQueries import *

@TestScenario
def isNull(self, connection):
    """Verify support for isNull function."""
    values = [
        "hello", b'\xe5\x8d\xb0'.decode('utf-8'),
        -1, 0, 255,
        1.0, 0.0, -1.0,
        datetime.date(2000, 12, 31), datetime.datetime(2000, 12, 31, 23, 59, 59),
    ]
    with Given("PyODBC connection"):
        for value in values:
            query = "SELECT isNull(?)"
            with When(f"I run '{query}' with {repr(value)} parameter"):
                rows = connection.query(query, [value])
                expected = "[(0,)]"
                with Then(f"the result is {expected}", flags=TE):
                    assert repr(rows) == expected, error("result did not match")

@TestScenario
@Requirements(RQ_SRS_003_ParameterizedQueries_DataType_Select_Nullable_NULL("1.0"))
def Null(self, connection):
    """Verify support for handling NULL value."""
    with Given("PyODBC connection"):
        query = "SELECT isNull(?)"
        with When(f"I run '{query}' with [None] parameter", flags=TE):
            rows = connection.query(query, [None])
            expected = "[(1,)]"
            with Then(f"the result is {expected}", flags=TE):
                assert repr(rows) == expected, error("result did not match")

        query = "SELECT arrayReduce('count', [?, ?])"
        with When(f"I run '{query}' with [None, None] parameter", flags=TE):
            rows = connection.query(query, [None, None])
            expected = "[(0,)]"
            with Then(f"the result is {expected}", flags=TE):
                assert repr(rows) == expected, error("result did not match")

        query = "SELECT arrayReduce('count', [1, ?, ?])"
        with When(f"I run '{query}' with [1, None, None])", flags=TE):
            rows = connection.query(query, [1, None, None])
            expected = "[(1,)]"
            with Then(f"the result is {expected}", flags=TE):
                assert repr(rows) == expected, error("result did not match")

@TestFeature
@Name("functions and values")
def funcvalues(self, nullable=False):
    """Check clickhouse-odbc driver support for parameterized
    queries with functions and values using pyodbc connector.
    """
    with Logs() as logs, PyODBCConnection(logs=logs) as connection:
        args = {"connection": connection}

        Scenario("isNull", run=isNull, args=args, flags=TE)
        Scenario("Null", run=Null, args=args, flags=TE)
