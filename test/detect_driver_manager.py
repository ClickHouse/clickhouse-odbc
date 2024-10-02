#!/usr/bin/env python
# -*- coding: utf-8 -*-

import re
import pyodbc

try:
    connection = pyodbc.connect("DSN=__nonexistent_dsn__")
except pyodbc.Error as error:
    result = re.search(r"\[([^\[\]]+)]\[Driver Manager]", str(error))
    if result and len(result.groups()) >= 1:
        print(result.group(1))
    else:
        raise
