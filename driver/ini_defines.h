#include "platform.h"

#define LARGE_REGISTRY_LEN 4096 /// used for special cases */
#define MEDIUM_REGISTRY_LEN 256 /// normal size for user, database, etc.
#define SMALL_REGISTRY_LEN 10   /// for 1/0 settings

#define INI_KDESC TEXT("Description") /* Data source description */
#define INI_DATABASE TEXT("Database") /* Database Name */
#define INI_URL TEXT("Url")           /* Full url of server running the ClickHouse service */
#define INI_SERVER TEXT("Server")     /* Name of Server running the ClickHouse service */
#define INI_UID TEXT("UID")           /* Default User Name */
#define INI_USERNAME TEXT("Username") /* Default User Name */
#define INI_PASSWORD TEXT("Password") /* Default Password */
#define INI_PORT TEXT("Port")         /* Port on which the ClickHouse is listening */
#define INI_READONLY TEXT("ReadOnly") /* Database is read only */
//#define INI_PROTOCOL TEXT("Protocol") /* What protocol (6.2) */
#define INI_TIMEOUT TEXT("Timeout")
#define INI_SSLMODE TEXT("SSLMode") /* Use 'require' for https connections */
#define INI_DSN TEXT("ClickHouse")
#define INI_STRINGMAXLENGTH TEXT("StringMaxLength")
#define INI_TRACE TEXT("Trace")
#define INI_TRACEFILE TEXT("TraceFile")

#ifndef WIN32
#    define ODBC_INI TEXT(".odbc.ini")
#    define ODBCINST_INI TEXT("odbcinst.ini")
#else
#    define ODBC_INI TEXT("ODBC.INI")
#    define ODBCINST_INI TEXT("ODBCINST.INI")
#endif
