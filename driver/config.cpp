#include "config.h"

#include <odbcinst.h>
#include <string.h>


ConnInfo::ConnInfo()
{
#define ZERO_FIELD(name) \
    memset(name, 0, sizeof(name));

    ZERO_FIELD(dsn);
    ZERO_FIELD(desc);
    ZERO_FIELD(drivername);
    ZERO_FIELD(server);
    ZERO_FIELD(database);
    ZERO_FIELD(username);
    ZERO_FIELD(password);
    ZERO_FIELD(port);
    ZERO_FIELD(sslmode);
    ZERO_FIELD(onlyread);
    ZERO_FIELD(timeout);
    ZERO_FIELD(show_system_tables);
    ZERO_FIELD(translation_dll);
    ZERO_FIELD(translation_option);
    ZERO_FIELD(conn_settings);

#undef ZERO_FIELD
}

void getDSNinfo(ConnInfo * ci, bool overwrite)
{
#if UNICODE
    //using to_const_char_type = LPCWSTR;
    using to_char_type = LPWSTR;
#else
    //using to_const_char_type = LPCSTR;
    using to_char_type = LPSTR;
#endif

#define GET_CONFIG(NAME, INI_NAME, DEFAULT) if (ci->NAME[0] == '\0' || overwrite) \
    SQLGetPrivateProfileString(reinterpret_cast<to_char_type>(ci->dsn), INI_NAME, TEXT(DEFAULT), reinterpret_cast<to_char_type>(ci->NAME), sizeof(ci->NAME), ODBC_INI);

    GET_CONFIG(desc, INI_KDESC, "");
    GET_CONFIG(server, INI_SERVER, "");
    GET_CONFIG(database, INI_DATABASE, "");
    GET_CONFIG(username, INI_USERNAME, "");
    GET_CONFIG(port, INI_PORT, "");
    GET_CONFIG(onlyread, INI_READONLY, "");
    GET_CONFIG(password, INI_PASSWORD, "");
    GET_CONFIG(timeout, INI_TIMEOUT, "30");
    GET_CONFIG(sslmode, INI_SSLMODE, "");

#undef GET_CONFIG

}
