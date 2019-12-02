#!/usr/bin/env python3
#  Copyright 2019, Altinity LTD. All Rights Reserved.
#
#  All information contained herein is, and remains the property
#  of Altinity LTD. Any dissemination of this information or
#  reproduction of this material is strictly forbidden unless
#  prior written permission  is obtained from Altinity LTD.
#
import datetime
import decimal

from testflows.core import TestScenario, Given, When, Then
from testflows.core import TE, MAN
from utils import PyODBCConnection

@TestScenario
def sanity():
    """clickhouse-odbc driver sanity suite to check support of parameterized
    queries using pyodbc connector.
    """
    with PyODBCConnection() as conn:
        with Given("PyODBC connection"):
            def query(query, *args, **kwargs):
                """Execute a query and check that it does not
                raise an exception.
                """
                with When(f"I execute '{query}'", flags=TE):
                    with Then("it works"):
                        conn.query(query, *args, **kwargs)

            with When("I want to do sanity check"):
                query("SELECT 1")

            table_schema = (
                "CREATE TABLE ps (i UInt8, ni Nullable(UInt8), s String, d Date, dt DateTime, "
                "f Float32, dc Decimal32(3), fs FixedString(8)) ENGINE = Memory"
            )

            with Given("table", description=f"Table schema {table_schema}", flags=MAN):
                query("DROP TABLE IF EXISTS ps", fetch=False)
                query(table_schema, fetch=False)
                try:
                    with When("I want to insert a couple of rows"):
                        query("INSERT INTO ps VALUES (1, NULL, 'Hello, world', '2005-05-05', '2005-05-05 05:05:05', "
                            "1.333, 10.123, 'fstring0')", fetch=False)
                        query("INSERT INTO ps VALUES (2, NULL, 'test', '2019-05-25', '2019-05-25 15:00:00', "
                            "1.433, 11.124, 'fstring1')", fetch=False)
                        query("SELECT * FROM ps")

                    with When("I want to select using parameter of type UInt8", flags=TE):
                        query("SELECT * FROM ps WHERE i = ? ORDER BY i, s, d", [1])

                    with When("I want to select using parameter of type Nullable(UInt8)", flags=TE):
                        query("SELECT * FROM ps WHERE ni = ? ORDER BY i, s, d", [None])

                    with When("I want to select using parameter of type String", flags=TE):
                        query("SELECT * FROM ps WHERE s = ? ORDER BY i, s, d", ["Hello, world"])

                    with When("I want to select using parameter of type Date", flags=TE):
                        query("SELECT * FROM ps WHERE d = ? ORDER BY i, s, d", [datetime.date(2019,5,25)])

                    with When("I want to select using parameter of type DateTime", flags=TE):
                        query("SELECT * FROM ps WHERE dt = ? ORDER BY i, s, d", [datetime.datetime(2005, 5, 5, 5, 5, 5)])

                    with When("I want to select using parameter of type Float32", flags=TE):
                        query("SELECT * FROM ps WHERE f = ? ORDER BY i, s, d", [1.333])

                    with When("I want to select using parameter of type Decimal32(3)", flags=TE):
                        query("SELECT * FROM ps WHERE dc = ? ORDER BY i, s, d", [decimal.Decimal('10.123')])

                    with When("I want to select using parameter of type FixedString(8)", flags=TE):
                        query("SELECT * FROM ps WHERE fs = ? ORDER BY i, s, d", [u"fstring0"])

                    with When("I want to select using parameters of type UInt8 and String", flags=TE):
                        query("SELECT * FROM ps WHERE i = ? and s = ? ORDER BY i, s, d", [2, "test"])

                    with When("I want to select using parameters of type UInt8, String, and Date", flags=TE):
                        query("SELECT * FROM ps WHERE i = ? and s = ? and d = ? ORDER BY i, s, d",
                            [2, "test", datetime.date(2019,5,25)])
                finally:
                    query("DROP TABLE ps", fetch=False)
