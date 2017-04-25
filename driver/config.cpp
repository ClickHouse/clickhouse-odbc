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
    ZERO_FIELD(show_system_tables);
    ZERO_FIELD(translation_dll);
    ZERO_FIELD(translation_option);
    ZERO_FIELD(conn_settings);

#undef ZERO_FIELD
}

void getDSNinfo(ConnInfo * ci, bool overwrite)
{
    if (ci->desc[0] == '\0' || overwrite)
        SQLGetPrivateProfileString(ci->dsn, INI_KDESC, TEXT(""), ci->desc, sizeof(ci->desc), ODBC_INI);

    if (ci->server[0] == '\0' || overwrite)
        SQLGetPrivateProfileString(ci->dsn, INI_SERVER, TEXT("localhost"), ci->server, sizeof(ci->server), ODBC_INI);

    if (ci->database[0] == '\0' || overwrite)
        SQLGetPrivateProfileString(ci->dsn, INI_DATABASE, TEXT("default"), ci->database, sizeof(ci->database), ODBC_INI);

    if (ci->username[0] == '\0' || overwrite)
        SQLGetPrivateProfileString(ci->dsn, INI_USERNAME, TEXT("default"), ci->username, sizeof(ci->username), ODBC_INI);

    if (ci->port[0] == '\0' || overwrite)
        SQLGetPrivateProfileString(ci->dsn, INI_PORT, TEXT("8123"), ci->port, sizeof(ci->port), ODBC_INI);

    if (ci->onlyread[0] == '\0' || overwrite)
        SQLGetPrivateProfileString(ci->dsn, INI_READONLY, TEXT(""), ci->onlyread, sizeof(ci->onlyread), ODBC_INI);
}
