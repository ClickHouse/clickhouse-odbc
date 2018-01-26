#pragma once

#if defined(__linux__)
#   define _linux_
#elif defined(_WIN64)
#   define _win64_
#   define _win32_
#elif defined(__WIN32__) || defined(_WIN32)
#   define _win32_
#elif defined(__APPLE__)
#   define _darwin_
#   define _unix_
#endif

#if defined(_win32_) || defined(_win64_)
#   define _win_
#endif

#if defined(_linux_)
#   define _unix_
#endif

#if defined(_MSC_VER)
#   undef NOMINMAX
#   define NOMINMAX
#   include <basetsd.h>
#   define ssize_t SSIZE_T
#   define HAVE_SSIZE_T 1
#
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#if defined (_win_)
#   ifdef UNICODE
#       include <sqlucode.h>

#       define strcpy wcscpy_s
#       define stricmp _wcsicmp
#       define strncpy wcsncpy
#   else
#       define strcpy strcpy_s
#       define stricmp _stricmp
#   endif
#else
#   define LPCTSTR const char*
#   define TEXT(value)  value
#endif

#if defined (UNICODE)
#   if defined (_win_)
#       define SIZEOF_CHAR  sizeof(uint_least16_t)
#   else
#       define SIZEOF_CHAR  sizeof(char16_t)
#   endif
#else
#   define SIZEOF_CHAR sizeof(char)
#endif
