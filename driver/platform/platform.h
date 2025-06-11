#pragma once

#if defined(_UNICODE) && _UNICODE == 0
#    undef _UNICODE
#endif

#if defined(UNICODE) && UNICODE == 0
#    undef UNICODE
#endif

#if defined(_UNICODE) || defined(UNICODE)
#    undef _UNICODE
#    define _UNICODE 1

#    undef UNICODE
#    define UNICODE 1
#endif

#if defined(UNICODE)
#    include "driver/platform/config_cmakew.h"
#else
#    include "driver/platform/config_cmake.h"
#endif

#if defined(__linux__)
#    define _linux_ 1
#elif defined(_WIN64) || defined(WIN64)
#    define _win64_ 1
#    define _win32_ 1
#elif defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
#    define _win32_ 1
#elif defined(__APPLE__)
#    define _darwin_ 1
#endif

#if defined(_win32_) || defined(_win64_)
#    define _win_ 1
#endif

#if defined(_linux_) || defined(_darwin_)
#    define _unix_ 1
#endif

#if defined(_MSC_VER)
#    undef NOMINMAX
#    define NOMINMAX
#    include <basetsd.h>
#    define ssize_t SSIZE_T
#    define HAVE_SSIZE_T 1
#
#    include <winsock2.h>
// DO NOT REORDER
#    include <windows.h>
//#    include <ws2tcpip.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#if defined(_IODBCUNIX_H)
#    include <iodbcext.h>
#endif

#if defined(_win_)
#    if defined(UNICODE)
#        include <sqlucode.h>

#        define strcpy wcscpy_s
#        define stricmp _wcsicmp
//#       define strncpy wcsncpy
#    else
#        define strcpy strcpy_s
#        define stricmp _stricmp
#    endif
#endif

#define SIZEOF_CHAR sizeof(SQLTCHAR)

#if defined(_MSC_VER)
     // An entry in .def file still must be added for 32-bit targets.
#    define EXPORTED_FUNCTION(func) __pragma(comment(linker,"/export:"#func)) func
#elif defined(_win_)
#    define EXPORTED_FUNCTION(func) __declspec(dllexport) func
#else
#    define EXPORTED_FUNCTION(func) __attribute__((visibility("default"))) func
#endif

#if defined(UNICODE)
#    define EXPORTED_FUNCTION_MAYBE_W(func) EXPORTED_FUNCTION(func##W)
#else
#    define EXPORTED_FUNCTION_MAYBE_W(func) EXPORTED_FUNCTION(func)
#endif

#if (ODBCVER >= 0x0380)
#    define SQL_DRIVER_AWARE_POOLING_CAPABLE 0x00000001L
#endif /* ODBCVER >= 0x0300 */

#define SQL_DRIVER_AWARE_POOLING_SUPPORTED 10024

#if (ODBCVER >= 0x0380)
#    define SQL_ASYNC_NOTIFICATION 10025
// Possible values for SQL_ASYNC_NOTIFICATION
#    define SQL_ASYNC_NOTIFICATION_NOT_CAPABLE 0x00000000L
#    define SQL_ASYNC_NOTIFICATION_CAPABLE 0x00000001L
#endif // ODBCVER >= 0x0380

#if defined(UNICODE)
#    define DRIVER_FILE_NAME "CLICKHOUSEODBCW.DLL"
#else
#    define DRIVER_FILE_NAME "CLICKHOUSEODBC.DLL"
#endif

// Custom attributes and other macros that mimic and extend standard ODBC API specs.

#define CH_SQL_OFFSET   30000

#define CH_SQL_ATTR_DRIVERLOG            (SQL_ATTR_TRACE + CH_SQL_OFFSET)
#define CH_SQL_ATTR_DRIVERLOGFILE        (SQL_ATTR_TRACEFILE + CH_SQL_OFFSET)

