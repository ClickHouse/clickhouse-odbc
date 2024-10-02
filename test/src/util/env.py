import logging
import os

LOGGER = logging.getLogger(__name__)
DEFAULT_DSN = "ClickHouse DSN (ANSI)"


def read_dsn_from_env():
    env_dsn = os.getenv("DSN")
    if env_dsn is None:
        LOGGER.info(f"Setting DSN to default value: {DEFAULT_DSN}")
        return DEFAULT_DSN
    return env_dsn


DSN = read_dsn_from_env()
CLICKHOUSE_LOG_PATH = os.getenv("CLICKHOUSE_LOG", "/var/log/clickhouse-server/clickhouse-server.log")
ODBC_DRIVER_TRACE_LOG_PATH = os.getenv("ODBC_DRIVER_TRACE_LOG", "/tmp/clickhouse-odbc-driver-trace.log")
ODBC_DRIVER_W_TRACE_LOG_PATH = os.getenv("ODBC_DRIVER_W_TRACE_LOG", "/tmp/clickhouse-odbc-driver-w-trace.log")
ODBC_MANAGER_TRACE_LOG_PATH = os.getenv("ODBC_MANAGER_TRACE_LOG", "/tmp/odbc-driver-manager-trace.log")
