#pragma once

#include "driver/utils/utils.h"

#include <string>

using key_value_map_t = std::map<std::string, std::string, UTF8CaseInsensitiveCompare>;

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
};

bool DSNExists(Environment & env, const std::string & target_dsn);
void readDSNinfo(ConnInfo * ci, bool overwrite);
void writeDSNinfo(const ConnInfo * ci);
key_value_map_t readDSNInfo(const std::string & dsn);
key_value_map_t readConnectionString(const std::string & connection_string);
