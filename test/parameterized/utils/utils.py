#  Copyright 2019, Altinity LTD. All Rights Reserved.
#
#  All information contained herein is, and remains the property
#  of Altinity LTD. Any dissemination of this information or
#  reproduction of this material is strictly forbidden unless
#  prior written permission  is obtained from Altinity LTD.
#
import os
import time
import pyodbc

import testflows.settings as settings

from contextlib import contextmanager
from testflows.core import note, exception, fail
from testflows.connect import Shell

clickhouse_log_path = os.getenv("CLICKHOUSE_LOG", "/var/log/clickhouse-server/clickhouse-server.log")
odbc_driver_trace_log_path = os.getenv("ODBC_DRIVER_TRACE_LOG", "/tmp/clickhouse-odbc-driver-trace.log")
odbc_driver_w_trace_log_path = os.getenv("ODBC_DRIVER_W_TRACE_LOG", "/tmp/clickhouse-odbc-driver-w-trace.log")
odbc_manager_trace_log_path = os.getenv("ODBC_MANAGER_TRACE_LOG", "/tmp/odbc-driver-manager-trace.log")

@contextmanager
def Logs():
    """ClickHouse and ODBC driver logs context manager.
    """
    class _Logs:
        def __init__(self, *args):
            self.logs = args

        def read(self, timeout=None):
            for l in self.logs:
                l.readlines(timeout=timeout)

    if not settings.debug:
        yield None
    else:
        with Shell(name="clickhouse-server.log") as bash0, \
            Shell(name="odbc-driver-trace.log") as bash1, \
            Shell(name="odbc-driver-w-trace.log") as bash2, \
            Shell(name="odbc-manager-trace.log") as bash3:

            bash1(f"touch {odbc_driver_trace_log_path}")
            bash2(f"touch {odbc_driver_w_trace_log_path}")
            bash3(f"touch {odbc_manager_trace_log_path}")

            with bash0(f"tail -f {clickhouse_log_path}", asyncronous=True, name="") as clickhouse_log, \
                bash1(f"tail -f {odbc_driver_trace_log_path}", asyncronous=True, name="") as odbc_driver_log, \
                bash2(f"tail -f {odbc_driver_w_trace_log_path}", asyncronous=True, name="") as odbc_driver_w_log, \
                bash3(f"tail -f {odbc_manager_trace_log_path}", asyncronous=True, name="") as odbc_manager_log:
                logs = _Logs(clickhouse_log, odbc_driver_log, odbc_driver_w_log, odbc_manager_log)
                logs.read()
                yield logs


@contextmanager
def PyODBCConnection(encoding="utf-8", logs=None):
    """PyODBC connector context manager.
    """
    dsn = os.getenv("DSN", "clickhouse_localhost")
    note(f"Using DNS={dsn}")
    connection = pyodbc.connect(f"DSN={dsn};")
    try:
        class _Connection():
            def __init__(self, connection, encoding, logs=None):
                self.connection = connection
                self.logs = logs
                self.encoding = encoding
                self.connection.setencoding(encoding=self.encoding)
                if self.logs:
                    self.logs.read()

            def query(self, q, params=[], fetch=True):
                try:
                    note(f"query: {q}")
                    cursor = self.connection.cursor()
                    cursor.execute(q, *params)
                    if fetch:
                        rows = cursor.fetchall()
                        for row in rows:
                            note(row)
                        return rows
                except pyodbc.Error as exc:
                    exception()
                    fail(str(exc))
                finally:
                    if self.logs and settings.debug:
                        # sleep 0.5 sec to let messages to be written to the logs
                        time.sleep(0.5)
                        self.logs.read(timeout=0.1)

        yield _Connection(connection, encoding, logs=logs)
    finally:
        connection.close()
