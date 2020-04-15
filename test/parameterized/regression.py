#!/usr/bin/env python3
import os

from testflows.core import TestModule, TestFeature, Feature, Requirements
from testflows.core import Name, Fail, Error, load, run
from testflows.core import main, TE
from requirements.QA_SRS003_ParameterizedQueries import *

@TestFeature
@Requirements(
    RQ_SRS_003_ParameterizedQueries("1.0"),
    RQ_SRS_003_ParameterizedQueries_pyodbc("1.0"),
    RQ_SRS_003_ParameterizedQueries_Syntax_Select_Parameters("1.0")
)
def parameterized(self):
    """Test suite for clickhouse-odbc support of parameterized queries.
    """
    dsn = os.getenv("DSN", "ClickHouse DSN (ANSI)")
    with Feature(f"{dsn}", flags=TE):
        run(test=load("parameterized.sanity", test="sanity"), flags=TE)
        run(test=load("parameterized.datatypes", test="datatypes"), flags=TE)
        run(test=load("parameterized.datatypes", test="nullable"), flags=TE)
        run(test=load("parameterized.funcvalues", test="funcvalues"), flags=TE)

@TestModule
def regression(self):
    """The regression module for clickhouse-odbc driver.
    """
    run(test=parameterized, flags=TE)

if main():
    xfails = {
        "/regression/parameterized/:/sanity/PyODBC connection/table/I want to select using parameter of type Nullable:":
            [(Fail, "Nullable type still not supported")],

        "/regression/parameterized/:/sanity/PyODBC connection/table/I want to select using parameter of type Decimal:":
            [(Fail, "Decimal type still not supported")],

        "/regression/parameterized/*/datatypes/Int64/"
        "PyODBC connection/parameters/table with a column of data type Int64/"
        "*/I select value -9223372036854775808/*":
            [(Fail, "Int64 large negative value not supported")],

        "/regression/parameterized/*/datatypes/Float32/"
        "PyODBC connection/parameters/table with a column of data type Float32/"
        "*/I select value 13.26/*":
            [(Fail, "Selecting Float32 values is not supported")],

        "/regression/parameterized/*/datatypes/Float:/"
        "PyODBC connection/parameters/table with a column of data type Float:/"
        "*/I select value nan":
            [(Fail, "Selecting value nan is not supported")],

        "/regression/parameterized/*/datatypes/FixedString/:/"
        "PyODBC connection/parameters/table with a column of data type FixedString:/"
        "I have values:/I select value:":
            [(Fail, "Selecting FixedString is not supported due to lack of toFixedString conversion")],

        "/regression/parameterized/*/datatypes/Decimal:":
            [(Fail, "Decimal type still not supported")],

        "*/I select value 18446744073709551615":
            [(Error, "UInt64 large value not supported")],

        "/regression/parameterized/*/datatypes/IPv4":
            [(Fail, "IPv4 is not supported")],

        "/regression/parameterized/*/datatypes/IPv6":
            [(Fail, "IPv6 is not supported")],

        "/regression/parameterized/*/datatypes/UUID"
        "/PyODBC connection/parameters/table with a column of data type UUID/"
        "I have values */I select value *":
            [(Fail, "UUID value selection is not supported due to incorrect type convertion to UInt128")],

        "/regression/parameterized/*/datatypes/String/binary":
            [
                (Error, "Test procedure is not correct"),
                (Fail, "Test procedure is not correct")
            ],

        "/regression/parameterized/:/nullable/datatypes/:":
            [
                (Error, "Nullables are not supported"),
                (Fail, "Nullables are not supported")
            ],

        "/regression/parameterized/:/nullable/datatypes/String/empty/utf-8":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/nullable/datatypes/String/empty/ascii":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/nullable/datatypes/String/utf8":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/nullable/datatypes/String/ascii":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/nullable/datatypes/FixedString/utf8":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/nullable/datatypes/FixedString/ascii":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/nullable/datatypes/Enum/utf8":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/nullable/datatypes/Enum/ascii":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/functions and values/Null":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/datatypes/String/utf8":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/datatypes/FixedString/utf8":
            [ (Fail, "Known failure")],
        "/regression/parameterized/:/datatypes/Enum/utf8":
            [ (Fail, "Known failure")]
    }

    run(test=regression, xfails=xfails)
