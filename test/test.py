#!/usr/bin/env python

# sudo apt install -y python-pyodbc
# sudo pkg install -y py36-pyodbc

import pyodbc

connection = pyodbc.connect('DSN=clickhouse_localhost;')
cursor = connection.cursor()
cursor.execute("select * from system.build_options")
rows = cursor.fetchall()
for row in rows:
    print(row)
    