#pragma once

#include "platform.h"

#define LARGE_REGISTRY_LEN  4096    /// used for special cases */
#define MEDIUM_REGISTRY_LEN 256     /// normal size for user, database, etc.
#define SMALL_REGISTRY_LEN  10      /// for 1/0 settings

#define INI_KDESC           TEXT("Description") /* Data source description */
#define INI_DATABASE        TEXT("Database")    /* Database Name */
#define INI_SERVER          TEXT("Server")      /* Name of Server  running the ClickHouse service */
#define INI_UID             TEXT("UID")         /* Default User Name */
#define INI_USERNAME        TEXT("Username")    /* Default User Name */
#define INI_PASSWORD        TEXT("Password")    /* Default Password */
#define INI_PORT            TEXT("Port")        /* Port on which the ClickHouse is listening */
#define INI_READONLY        TEXT("ReadOnly")    /* Database is read only */

#ifndef WIN32
#   define ODBC_INI         ".odbc.ini"
#   define ODBCINST_INI     "odbcinst.ini"
#else
#   define ODBC_INI         TEXT("ODBC.INI")
#   define ODBCINST_INI     TEXT("ODBCINST.INI")
#endif

/**
 * Structure to hold all the connection attributes for a specific
 * connection (used for both registry and file, DSN and DRIVER)
 */
struct ConnInfo
{
    TCHAR       dsn[MEDIUM_REGISTRY_LEN];
    TCHAR       desc[MEDIUM_REGISTRY_LEN];
    TCHAR       drivername[MEDIUM_REGISTRY_LEN];
    TCHAR       server[MEDIUM_REGISTRY_LEN];
    TCHAR       database[MEDIUM_REGISTRY_LEN];
    TCHAR       username[MEDIUM_REGISTRY_LEN];
    TCHAR       password[MEDIUM_REGISTRY_LEN];
    TCHAR       port[SMALL_REGISTRY_LEN];
    TCHAR       sslmode[16];
    TCHAR       onlyread[SMALL_REGISTRY_LEN];
    TCHAR       show_system_tables[SMALL_REGISTRY_LEN];
    TCHAR       translation_dll[MEDIUM_REGISTRY_LEN];
    TCHAR       translation_option[SMALL_REGISTRY_LEN];
    TCHAR       conn_settings[MEDIUM_REGISTRY_LEN];
    signed char disallow_premature = -1;
    signed char allow_keyset = -1;
    signed char updatable_cursors = 0;
    signed char lf_conversion = -1;
    signed char true_is_minus1 = -1;
    signed char int8_as = -101;
    signed char bytea_as_longvarbinary = -1;
    signed char use_server_side_prepare = -1;
    signed char lower_case_identifier = -1;
    signed char rollback_on_error = -1;
    signed char force_abbrev_connstr = -1;
    signed char bde_environment = -1;
    signed char fake_mss = -1;
    signed char cvt_null_date_string = -1;
    signed char autocommit_public = SQL_AUTOCOMMIT_ON;
    signed char accessible_only = -1;
    signed char ignore_round_trip_time = -1;
    signed char disable_keepalive = -1;

    ConnInfo();
};

void getDSNinfo(ConnInfo * ci, bool overwrite);
