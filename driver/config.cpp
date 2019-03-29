#include "config.h"

#include <odbcinst.h>
#include <string.h>
#include "utils.h"

ConnInfo::ConnInfo() {
#define ZERO_FIELD(name) memset(name, 0, sizeof(name));

    ZERO_FIELD(dsn);
    ZERO_FIELD(desc);
    ZERO_FIELD(drivername);
    ZERO_FIELD(url);
    ZERO_FIELD(server);
    ZERO_FIELD(database);
    ZERO_FIELD(username);
    ZERO_FIELD(password);
    ZERO_FIELD(port);
    ZERO_FIELD(sslmode);
    ZERO_FIELD(onlyread);
    ZERO_FIELD(timeout);
    ZERO_FIELD(stringmaxlength);
    ZERO_FIELD(trace);
    ZERO_FIELD(tracefile);
    ZERO_FIELD(show_system_tables);
    ZERO_FIELD(translation_dll);
    ZERO_FIELD(translation_option);
    ZERO_FIELD(conn_settings);

#undef ZERO_FIELD
}

void getDSNinfo(ConnInfo * ci, bool overwrite) {
#define GET_CONFIG(NAME, INI_NAME, DEFAULT)          \
    if (ci->NAME[0] == '\0' || overwrite)            \
        FUNCTION_MAYBE_W(SQLGetPrivateProfileString) ( \
            ci->dsn, \
            static_cast<LPCTSTR>(static_cast<const void*>(INI_NAME)), \
            static_cast<LPCTSTR>(static_cast<const void*>(TEXT(DEFAULT))), \
            ci->NAME, \
            sizeof(ci->NAME), \
            static_cast<LPCTSTR>(static_cast<const void*>(ODBC_INI)) \
        );

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
    GET_CONFIG(tracefile, INI_TRACEFILE, LOG_DEFAULT_FILE);

#undef GET_CONFIG
}
