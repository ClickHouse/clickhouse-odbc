#pragma once

#include "ini_defines.h"
#include "unicode_t.h"

/**
 * Structure to hold all the connection attributes for a specific
 * connection (used for both registry and file, DSN and DRIVER)
 */
struct ConnInfo {
    MYTCHAR dsn[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR desc[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR drivername[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR url[LARGE_REGISTRY_LEN] = {};
    MYTCHAR server[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR database[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR username[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR password[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR port[SMALL_REGISTRY_LEN] = {};
    MYTCHAR sslmode[16] = {};
    MYTCHAR onlyread[SMALL_REGISTRY_LEN] = {};
    MYTCHAR timeout[SMALL_REGISTRY_LEN] = {};
    MYTCHAR stringmaxlength[SMALL_REGISTRY_LEN] = {};
    MYTCHAR show_system_tables[SMALL_REGISTRY_LEN] = {};
    MYTCHAR translation_dll[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR translation_option[SMALL_REGISTRY_LEN] = {};
    MYTCHAR conn_settings[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR trace[SMALL_REGISTRY_LEN] = {};
    MYTCHAR tracefile[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR privateKeyFile[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR certificateFile[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR caLocation[MEDIUM_REGISTRY_LEN] = {};

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

    //ConnInfo();
};

void getDSNinfo(ConnInfo * ci, bool overwrite);
