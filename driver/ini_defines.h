#include "platform.h"

#define LARGE_REGISTRY_LEN 4096 /// used for special cases */
#define MEDIUM_REGISTRY_LEN 256 /// normal size for user, database, etc.
#define SMALL_REGISTRY_LEN 10   /// for 1/0 settings

#define INI_KDESC "Description" /* Data source description */
#define INI_DATABASE "Database" /* Database Name */
#define INI_URL "Url"           /* Full url of server running the ClickHouse service */
#define INI_SERVER "Server"     /* Name of Server running the ClickHouse service */
#define INI_UID "UID"           /* Default User Name */
#define INI_USERNAME "Username" /* Default User Name */
#define INI_PASSWORD "Password" /* Default Password */
#define INI_PORT "Port"         /* Port on which the ClickHouse is listening */
#define INI_READONLY "ReadOnly" /* Database is read only */
//#define INI_PROTOCOL "Protocol" /* What protocol (6.2) */
#define INI_TIMEOUT "Timeout"
#define INI_SSLMODE "SSLMode"   /* Use 'require' for https connections */
#define INI_DSN "ClickHouse"
#define INI_STRINGMAXLENGTH "StringMaxLength"
#define INI_TRACE "Trace"
#define INI_TRACEFILE "TraceFile"

#ifdef _win_
#    define ODBC_INI "ODBC.INI"
#    define ODBCINST_INI "ODBCINST.INI"
#else
#    define ODBC_INI ".odbc.ini"
#    define ODBCINST_INI "odbcinst.ini"
#endif
