import logging
import os
import base64
from contextlib import contextmanager

import pyodbc

LOGGER = logging.getLogger(__name__)


@contextmanager
def shell_logs():
    """ClickHouse and ODBC driver logs context manager.
    """

    # rewrite without testflows.Shell if needed
    #
    # class _Logs:
    #     def __init__(self, *args):
    #         self.logs = args
    #
    #     def read(self, timeout=None):
    #         for log in self.logs:
    #             log.readlines(timeout=timeout)
    #
    # if not settings.debug:
    #     yield None
    # else:
    #     with Shell(name="clickhouse-server.log") as bash0, \
    #         Shell(name="odbc-driver-trace.log") as bash1, \
    #         Shell(name="odbc-driver-w-trace.log") as bash2, \
    #         Shell(name="odbc-manager-trace.log") as bash3:
    #
    #         bash1(f"touch {ODBC_DRIVER_TRACE_LOG_PATH}")
    #         bash2(f"touch {ODBC_DRIVER_W_TRACE_LOG_PATH}")
    #         bash3(f"touch {ODBC_MANAGER_TRACE_LOG_PATH}")
    #
    #         with bash0(f"tail -f {CLICKHOUSE_LOG_PATH}", asyncronous=True, name="") as clickhouse_log, \
    #             bash1(f"tail -f {ODBC_DRIVER_TRACE_LOG_PATH}", asyncronous=True, name="") as odbc_driver_log, \
    #             bash2(f"tail -f {ODBC_DRIVER_W_TRACE_LOG_PATH}", asyncronous=True, name="") as odbc_driver_w_log, \
    #             bash3(f"tail -f {ODBC_MANAGER_TRACE_LOG_PATH}", asyncronous=True, name="") as odbc_manager_log:
    #             logs = _Logs(clickhouse_log, odbc_driver_log, odbc_driver_w_log, odbc_manager_log)
    #             logs.read()
    #             yield logs


class PyODBCConnection:
    def __init__(self, connection: pyodbc.Connection, encoding, logs=None):
        self.connection = connection
        self.logs = logs
        self.encoding = encoding
        # encoding/decoding fix is required for iODBC (UTF-16 by default)
        self.connection.setencoding(encoding=self.encoding)
        self.connection.setdecoding(pyodbc.SQL_CHAR, encoding=self.encoding, ctype=pyodbc.SQL_CHAR)
        if self.logs:
            self.logs.read()

    def query(self, q, params=None, fetch=True):
        if params is None:
            params = []
        try:
            LOGGER.debug(f"Query: {q}")
            if len(params) > 0:
                LOGGER.debug(f"Params: {params}")
            cursor = self.connection.cursor()
            # self.connection.setencoding(encoding=self.encoding)
            cursor.execute(q, *params)
            if fetch:
                # self.connection.setencoding(encoding="utf-16")
                rows = cursor.fetchall()
                for row in rows:
                    LOGGER.debug(f"Row: {row}")
                return rows
        except pyodbc.Error as exc:
            raise exc
        finally:
            pass
            # if self.logs and settings.debug:
            #     # sleep 0.5 sec to let messages to be written to the logs
            #     time.sleep(0.5)
            #     self.logs.read(timeout=0.1)

    def insert(self, table_name: str, values: str):
        stmt = f"INSERT INTO {table_name} VALUES {values}"
        self.query(stmt, fetch=False)


@contextmanager
def pyodbc_connection(encoding="utf-8", logs=None):
    dsn = os.getenv("DSN", "ClickHouse DSN (ANSI)")
    LOGGER.debug(f"Using DNS={dsn}")
    connection = None
    try:
        connection = pyodbc.connect(f"DSN={dsn};")
        yield PyODBCConnection(connection, encoding, logs=logs)
    except Exception as e:
        LOGGER.error(f"Error: {e}")
        raise
    finally:
        if connection:
            connection.close()


@contextmanager
def create_table(connection: PyODBCConnection, table_name: str, schema: str):
    try:
        # We perform cleanup after executing the tests; however, the table might still
        # exist due to a crash or cancellation of a previous test run.
        # `CREATE OR REPLACE TABLE` would be ideal here, but it doesn't work on WSL or macOS—
        # see https://github.com/ClickHouse/ClickHouse/issues/49339.
        # Instead, we use `DROP TABLE IF EXISTS` followed by `CREATE TABLE`
        # to simulate `CREATE OR REPLACE TABLE`.
        connection.query(f"DROP TABLE IF EXISTS {table_name}", fetch=False)
        ddl = f"CREATE TABLE {table_name} ({schema}) ENGINE = Memory"
        connection.query(ddl, fetch=False)
        yield
    finally:
        connection.query(f"DROP TABLE IF EXISTS {table_name}", fetch=False)


def rows_as_values(rows: list[pyodbc.Row]) -> list:
    return list(map(lambda r: list(r)[0], rows))

def to_base64(string):
    return base64.b64encode(string.encode('utf-8')).decode('utf-8')
