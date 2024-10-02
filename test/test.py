#!/usr/bin/env python
# -*- coding: utf-8 -*-

# sudo apt install -y python-pyodbc
# sudo apt install -y python3-pyodbc
# sudo pkg install -y py36-pyodbc
# pip install pyodbc

from __future__ import print_function

import pyodbc
import sys
import os
import time

is_python_3 = (sys.version_info.major == 3)
is_windows = (os.name == 'nt')


def main():
    dsn = 'ClickHouse DSN (ANSI)'

    if len(sys.argv) >= 2:
        dsn = sys.argv[1]

    print("Using DSN=" + dsn)
    connection = getConnection('DSN=' + dsn)

    query(connection, "select * from system.build_options")
    query(connection,
          "SELECT *, (CASE WHEN (number == 1) THEN 'o' WHEN (number == 2) THEN 'two long string' WHEN (number == 3) THEN 'r' WHEN (number == 4) THEN NULL ELSE '-' END) FROM system.numbers LIMIT 6")
    # TODO query("SELECT 1, 'string', NULL")
    if is_python_3:
        query(connection, u"SELECT 'абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ'")
    query(connection,
          "SELECT -127,-128,-129,126,127,128,255,256,257,-32767,-32768,-32769,32766,32767,32768,65535,65536,65537,-2147483647,-2147483648,-2147483649,2147483646,2147483647,2147483648,4294967295,4294967296,4294967297,-9223372036854775807,-9223372036854775808,-9223372036854775809,9223372036854775806,9223372036854775807,9223372036854775808,18446744073709551615,18446744073709551616,18446744073709551617")
    query(connection, "SELECT 2147483647, 2147483648, 2147483647+1, 2147483647+10, 4294967295")
    query(connection, "SELECT * FROM system.contributors ORDER BY name LIMIT 10")

    error_timeout_test(dsn)


def error_timeout_test(dsn):
    # Do test of proper timeout in case of sql syntax error
    timeout = 30
    with getConnection('DSN={};TIMEOUT={}'.format(dsn, timeout)) as connectionWithTimeout:
        start = time.time()
        try:
            query(connectionWithTimeout, "SELECT * FROM system.non_existing_table")
        except pyodbc.Error as e:
            print("Got expected error: {}".format(e))
    end = time.time()
    if timeout <= end - start:
        raise ValueError("Timeout for getting error is {} sec long - such delay isn't expected!".format(timeout))
    else:
        print("[SUCCEEDED] Got expected error within {:.3f} sec".format(end - start))


def getConnection(connectionString):
    connection = pyodbc.connect(connectionString)
    if is_python_3:
        if is_windows:
            connection.setdecoding(pyodbc.SQL_CHAR, encoding='utf-8', ctype=pyodbc.SQL_CHAR)
            connection.setdecoding(pyodbc.SQL_WCHAR, encoding='utf-16', ctype=pyodbc.SQL_WCHAR)
            connection.setdecoding(pyodbc.SQL_WMETADATA, encoding='utf-16', ctype=pyodbc.SQL_WCHAR)
            connection.setencoding(encoding='utf-8', ctype=pyodbc.SQL_CHAR)
        else:  # pyodbc doesn't support UCS-2 and UCS-4 conversions at the moment (and probably won't support ever?) so we retrieve everything in narrow-char utf-8
            connection.setdecoding(pyodbc.SQL_CHAR, encoding='utf-8', ctype=pyodbc.SQL_CHAR)
            connection.setdecoding(pyodbc.SQL_WCHAR, encoding='utf-8', ctype=pyodbc.SQL_CHAR)
            connection.setdecoding(pyodbc.SQL_WMETADATA, encoding='utf-8', ctype=pyodbc.SQL_CHAR)
            connection.setencoding(encoding='utf-8', ctype=pyodbc.SQL_CHAR)
    else:
        if is_windows:
            connection.setdecoding(pyodbc.SQL_CHAR, encoding='utf-8', ctype=pyodbc.SQL_CHAR, to=unicode)
            connection.setdecoding(pyodbc.SQL_WCHAR, encoding='utf-16', ctype=pyodbc.SQL_WCHAR, to=unicode)
            connection.setdecoding(pyodbc.SQL_WMETADATA, encoding='utf-16', ctype=pyodbc.SQL_WCHAR, to=unicode)
            connection.setencoding(str, encoding='utf-8', ctype=pyodbc.SQL_CHAR)
            connection.setencoding(unicode, encoding='utf-8', ctype=pyodbc.SQL_CHAR)
        else:  # pyodbc doesn't support UCS-2 and UCS-4 conversions at the moment (and probably won't support ever?) so we retrieve everything in narrow-char utf-8
            connection.setdecoding(pyodbc.SQL_CHAR, encoding='utf-8', ctype=pyodbc.SQL_CHAR, to=unicode)
            connection.setdecoding(pyodbc.SQL_WCHAR, encoding='utf-8', ctype=pyodbc.SQL_CHAR, to=unicode)
            connection.setdecoding(pyodbc.SQL_WMETADATA, encoding='utf-8', ctype=pyodbc.SQL_CHAR, to=unicode)
            connection.setencoding(str, encoding='utf-8', ctype=pyodbc.SQL_CHAR)
            connection.setencoding(unicode, encoding='utf-8', ctype=pyodbc.SQL_CHAR)

    return connection


def query(connection, q):
    print("{} :".format(q))
    cursor = connection.cursor()
    cursor.execute(q)
    rows = cursor.fetchall()
    for row in rows:
        print(row)


if __name__ == '__main__':
    main()
