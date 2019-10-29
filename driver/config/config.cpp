#include "config.h"
#include "driver.h"
#include "utils/utils.h"

#include <odbcinst.h>

#include <cstring>

void getDSNinfo(ConnInfo * ci, bool overwrite) {
    MYTCHAR odbc_ini[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR ini_name[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR default_value[LARGE_REGISTRY_LEN] = {};
    stringToTCHAR(ODBC_INI, odbc_ini);

#define GET_CONFIG(NAME, INI_NAME, DEFAULT)           \
    if (ci->NAME[0] == '\0' || overwrite) {           \
        stringToTCHAR(INI_NAME, ini_name);            \
        stringToTCHAR(DEFAULT, default_value);        \
        const auto read = SQLGetPrivateProfileString( \
            ci->dsn,                                  \
            ini_name,                                 \
            default_value,                            \
            ci->NAME,                                 \
            lengthof(ci->NAME),                       \
            odbc_ini                                  \
        );                                            \
        ci->NAME[read] = '\0';                        \
    }

	GET_CONFIG(desc,            INI_DESC,            INI_DESC_DEFAULT);
    GET_CONFIG(url,             INI_URL,             INI_URL_DEFAULT);
    GET_CONFIG(server,          INI_SERVER,          INI_SERVER_DEFAULT);
    GET_CONFIG(port,            INI_PORT,            INI_PORT_DEFAULT);
    GET_CONFIG(username,        INI_USERNAME,        INI_USERNAME_DEFAULT);
    GET_CONFIG(password,        INI_PASSWORD,        INI_PASSWORD_DEFAULT);
    GET_CONFIG(timeout,         INI_TIMEOUT,         INI_TIMEOUT_DEFAULT);
    GET_CONFIG(sslmode,         INI_SSLMODE,         INI_SSLMODE_DEFAULT);
    GET_CONFIG(database,        INI_DATABASE,        INI_DATABASE_DEFAULT);
    GET_CONFIG(onlyread,        INI_READONLY,        INI_READONLY_DEFAULT);
    GET_CONFIG(stringmaxlength, INI_STRINGMAXLENGTH, INI_STRINGMAXLENGTH_DEFAULT);
    GET_CONFIG(trace,           INI_TRACE,           INI_TRACE_DEFAULT);
    GET_CONFIG(tracefile,       INI_TRACEFILE,       INI_TRACEFILE_DEFAULT);

#undef GET_CONFIG
}

void writeDSNinfo(const ConnInfo * ci) {
    MYTCHAR odbc_ini[MEDIUM_REGISTRY_LEN] = {};
    MYTCHAR ini_name[MEDIUM_REGISTRY_LEN] = {};
    stringToTCHAR(ODBC_INI, odbc_ini);

#define WRITE_CONFIG(NAME, INI_NAME)                      \
    {                                                     \
        stringToTCHAR(INI_NAME, ini_name);                \
        const auto result = SQLWritePrivateProfileString( \
            ci->dsn,                                      \
            ini_name,                                     \
            ci->NAME,                                     \
            odbc_ini                                      \
        );                                                \
        if (!result)                                      \
            throw std::runtime_error(                     \
				"SQLWritePrivateProfileString failed");   \
    }

    WRITE_CONFIG(desc,            INI_DESC);
    WRITE_CONFIG(url,             INI_URL);
    WRITE_CONFIG(server,          INI_SERVER);
    WRITE_CONFIG(port,            INI_PORT);
    WRITE_CONFIG(username,        INI_USERNAME);
    WRITE_CONFIG(password,        INI_PASSWORD);
    WRITE_CONFIG(timeout,         INI_TIMEOUT);
    WRITE_CONFIG(sslmode,         INI_SSLMODE);
    WRITE_CONFIG(database,        INI_DATABASE);
    WRITE_CONFIG(onlyread,        INI_READONLY);
    WRITE_CONFIG(stringmaxlength, INI_STRINGMAXLENGTH);
    WRITE_CONFIG(trace,           INI_TRACE);
    WRITE_CONFIG(tracefile,       INI_TRACEFILE);

#undef WRITE_CONFIG
}
