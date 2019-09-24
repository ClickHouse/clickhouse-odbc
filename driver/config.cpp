#include "config.h"
#include "driver.h"
#include "utils.h"

#include <odbcinst.h>

#include <cstring>

void getDSNinfo(ConnInfo * ci, bool overwrite) {
#define GET_CONFIG(NAME, INI_NAME, DEFAULT)               \
    if (ci->NAME[0] == '\0' || overwrite) {               \
        MYTCHAR odbc_ini[MEDIUM_REGISTRY_LEN] = { };      \
        MYTCHAR ini_name[MEDIUM_REGISTRY_LEN] = { };      \
        MYTCHAR default_value[MEDIUM_REGISTRY_LEN] = { }; \
        stringToTCHAR(ODBC_INI, odbc_ini);                \
        stringToTCHAR(INI_NAME, ini_name);                \
        stringToTCHAR(DEFAULT, default_value);            \
        const auto read = SQLGetPrivateProfileString(     \
            ci->dsn,                                      \
            ini_name,                                     \
            default_value,                                \
            ci->NAME,                                     \
            lengthof(ci->NAME),                           \
            odbc_ini                                      \
        );                                                \
        ci->NAME[read] = '\0';                            \
    }

    SQLSetConfigMode(ODBC_BOTH_DSN);

    GET_CONFIG(desc, INI_KDESC, "");
    GET_CONFIG(url, INI_URL, "");
    GET_CONFIG(server, INI_SERVER, "");
    GET_CONFIG(database, INI_DATABASE, "");
    GET_CONFIG(username, INI_USERNAME, "");
    GET_CONFIG(port, INI_PORT, "");
    GET_CONFIG(onlyread, INI_READONLY, "");
    GET_CONFIG(password, INI_PASSWORD, "");
    GET_CONFIG(timeout, INI_TIMEOUT, "30");
    GET_CONFIG(sslmode, INI_SSLMODE, "");
    GET_CONFIG(stringmaxlength, INI_STRINGMAXLENGTH, "1048575");
    GET_CONFIG(trace, INI_TRACE, "");
    GET_CONFIG(tracefile, INI_TRACEFILE, DRIVER_TRACEFILE_DEFAULT);

#undef GET_CONFIG
}
