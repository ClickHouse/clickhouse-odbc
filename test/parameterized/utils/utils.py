import os
import time
import pyodbc

import testflows.settings as settings

from contextlib import contextmanager, ExitStack
from testflows.core import note, exception, fail
from testflows.connect import Shell

clickhouse_log_path = os.getenv("CLICKHOUSE_SERVER_LOG", None)
odbc_driver_log_path = os.getenv("ODBC_DRIVER_LOG", None)
odbc_manager_trace_log_path = os.getenv("ODBC_MANAGER_TRACE_LOG", None)

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
        with ExitStack as bash_stack:
            bash0, bash1, bash2 = None, None, None
            
            if clickhouse_log_path:
                bash0 = bash_stack.enter_context(Shell(name="clickhouse-server-log"))
            
            if odbc_driver_log_path:
                bash1 = bash_stack.enter_context(Shell(name="odbc-driver-log"))
                bash1(f"touch {odbc_driver_log_path}")
            
            if odbc_manager_trace_log_path:
                bash2 = bash_stack.enter_context(Shell(name="odbc-manager-trace-log"))
                bash2(f"touch {odbc_manager_trace_log_path}")

            with ExitStack as logs_stack:
                _logs = []
                if bash0:
                    _logs.append(logs_stack.enter_context(bash0(f"tail -f {clickhouse_log_path}", asyncronous=True, name="")))
                if bash1:
                    _logs.append(logs_stack.enter_context(bash1(f"tail -f {odbc_driver_log_path}", asyncronous=True, name="")))
                if bash2:
                    _logs.append(logs_stack.enter_context(bash2(f"tail -f {odbc_manager_trace_log_path}", asyncronous=True, name="")))
                logs = _Logs(*_logs)
                logs.read()
                yield logs


@contextmanager
def PyODBCConnection(encoding="utf-8", logs=None):
    """PyODBC connector context manager.
    """
    dsn = os.getenv("DSN", "ClickHouse DSN (ANSI)")
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
