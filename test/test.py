#!/usr/bin/env python
# -*- coding: utf-8 -*-

# sudo apt install -y python-pyodbc
# sudo pkg install -y py36-pyodbc

import pyodbc
import sys
reload(sys)
sys.setdefaultencoding('utf8')


connection = pyodbc.connect('DSN=clickhouse_localhost;')

def query(q):
    print(q + " :")
    cursor = connection.cursor()
    cursor.execute(q)
    rows = cursor.fetchall()
    for row in rows:
        print(row)

query("select * from system.build_options")
query("SELECT 'абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ'")
