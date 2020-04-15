#!/usr/bin/env python
# -*- coding: utf-8 -*-

# sudo apt install -y python-pyodbc
# sudo apt install -y python3-pyodbc
# sudo pkg install -y py36-pyodbc
# pip install pyodbc

import pyodbc
import sys
import os

dsn = 'ClickHouse DSN (ANSI)'

if len(sys.argv) >= 2:
    dsn = sys.argv[1]

is_python_3 = (sys.version_info.major == 3)
is_windows = (os.name == 'nt')

print("Using DSN=" + dsn)
connection = pyodbc.connect('DSN=' + dsn)

if is_python_3:
    if is_windows:
        connection.setdecoding(pyodbc.SQL_CHAR, encoding='utf-8', ctype=pyodbc.SQL_CHAR)
        connection.setdecoding(pyodbc.SQL_WCHAR, encoding='utf-16', ctype=pyodbc.SQL_WCHAR)
        connection.setdecoding(pyodbc.SQL_WMETADATA, encoding='utf-16', ctype=pyodbc.SQL_WCHAR)
        connection.setencoding(encoding='utf-8', ctype=pyodbc.SQL_CHAR)
    else: # pyodbc doesn't support UCS-2 and UCS-4 conversions at the moment (and probably won't support ever?) so we retrieve everything in narrow-char utf-8
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
    else: # pyodbc doesn't support UCS-2 and UCS-4 conversions at the moment (and probably won't support ever?) so we retrieve everything in narrow-char utf-8
        connection.setdecoding(pyodbc.SQL_CHAR, encoding='utf-8', ctype=pyodbc.SQL_CHAR, to=unicode)
        connection.setdecoding(pyodbc.SQL_WCHAR, encoding='utf-8', ctype=pyodbc.SQL_CHAR, to=unicode)
        connection.setdecoding(pyodbc.SQL_WMETADATA, encoding='utf-8', ctype=pyodbc.SQL_CHAR, to=unicode)
        connection.setencoding(str, encoding='utf-8', ctype=pyodbc.SQL_CHAR)
        connection.setencoding(unicode, encoding='utf-8', ctype=pyodbc.SQL_CHAR)

def query(q):
    print("{} :".format(q))
    cursor = connection.cursor()
    cursor.execute(q)
    rows = cursor.fetchall()
    for row in rows:
        print(row)

query("select * from system.build_options")
query("SELECT *, (CASE WHEN (number == 1) THEN 'o' WHEN (number == 2) THEN 'two long string' WHEN (number == 3) THEN 'r' WHEN (number == 4) THEN NULL ELSE '-' END) FROM system.numbers LIMIT 6")
#TODO query("SELECT 1, 'string', NULL")
if is_python_3:
    query(u"SELECT 'абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ'")
query("SELECT -127,-128,-129,126,127,128,255,256,257,-32767,-32768,-32769,32766,32767,32768,65535,65536,65537,-2147483647,-2147483648,-2147483649,2147483646,2147483647,2147483648,4294967295,4294967296,4294967297,-9223372036854775807,-9223372036854775808,-9223372036854775809,9223372036854775806,9223372036854775807,9223372036854775808,18446744073709551615,18446744073709551616,18446744073709551617")
query("SELECT 2147483647, 2147483648, 2147483647+1, 2147483647+10, 4294967295")
query("SELECT * FROM system.contributors ORDER BY name LIMIT 10")
