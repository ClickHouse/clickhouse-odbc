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
};

void getDSNinfo(ConnInfo * ci, bool overwrite);
void writeDSNinfo(const ConnInfo * ci);
