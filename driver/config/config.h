#pragma once

#include <string>

/**
 * Structure to hold all the connection attributes for a specific
 * connection (used for both registry and file, DSN and DRIVER)
 */
struct ConnInfo {
    std::string dsn;
    std::string desc;
    std::string drivername;
    std::string url;
    std::string server;
    std::string database;
    std::string username;
    std::string password;
    std::string port;
    std::string sslmode;
    std::string onlyread;
    std::string timeout;
    std::string stringmaxlength;
    std::string show_system_tables;
    std::string translation_dll;
    std::string translation_option;
    std::string conn_settings;
    std::string driverlog;
    std::string driverlogfile;
    std::string privateKeyFile;
    std::string certificateFile;
    std::string caLocation;

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
};

void getDSNinfo(ConnInfo * ci, bool overwrite);
void writeDSNinfo(const ConnInfo * ci);
