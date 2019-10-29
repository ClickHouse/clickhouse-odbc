#pragma once

#include "driver/platform/platform.h"

#define LARGE_REGISTRY_LEN 4096 /// used for special cases */
#define MEDIUM_REGISTRY_LEN 256 /// normal size for user, database, etc.
#define SMALL_REGISTRY_LEN 10   /// for 1/0 settings

#define INI_DRIVER          "Driver"
#define INI_DSN             "DSN"
#define INI_DESC            "Description"     /* Data source description */
#define INI_URL             "Url"             /* Full url of server running the ClickHouse service */
#define INI_PROTOCOL        "Protocol"        /* What protocol (6.2) */
#define INI_SERVER          "Server"          /* Name of Server running the ClickHouse service */
#define INI_PORT            "Port"            /* Port on which the ClickHouse is listening */
#define INI_UID             "UID"             /* Default User Name */
#define INI_USERNAME        "Username"        /* Default User Name */
#define INI_PWD             "PWD"             /* Default Password */
#define INI_PASSWORD        "Password"        /* Default Password */
#define INI_TIMEOUT         "Timeout"         /* Connection timeout */
#define INI_SSLMODE         "SSLMode"         /* Use 'require' for https connections */
#define INI_DATABASE        "Database"        /* Database Name */
#define INI_READONLY        "ReadOnly"        /* Database is read only */
#define INI_STRINGMAXLENGTH "StringMaxLength"
#define INI_TRACE           "Trace"
#define INI_TRACEFILE       "TraceFile"

#define INI_DSN_DEFAULT             "ClickHouseDSN_localhost"
#define INI_DESC_DEFAULT            ""
#define INI_URL_DEFAULT             ""
#define INI_SERVER_DEFAULT          ""
#define INI_PORT_DEFAULT            ""
#define INI_USERNAME_DEFAULT        ""
#define INI_PASSWORD_DEFAULT        ""
#define INI_TIMEOUT_DEFAULT         "30"
#define INI_SSLMODE_DEFAULT         ""
#define INI_DATABASE_DEFAULT        ""
#define INI_READONLY_DEFAULT        ""
#define INI_STRINGMAXLENGTH_DEFAULT "1048575"

#ifdef NDEBUG
#    define INI_TRACE_DEFAULT "off"
#else
#    define INI_TRACE_DEFAULT "on"
#endif

#ifdef _win_
#    define INI_TRACEFILE_DEFAULT "\\temp\\clickhouse-odbc.log"
#else
#    define INI_TRACEFILE_DEFAULT "/tmp/clickhouse-odbc.log"
#endif

#ifdef _win_
#    define ODBC_INI "ODBC.INI"
#    define ODBCINST_INI "ODBCINST.INI"
#else
#    define ODBC_INI ".odbc.ini"
#    define ODBCINST_INI "odbcinst.ini"
#endif
