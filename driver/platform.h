#pragma once

#include <type_traits>

//#if __has_include("config_cmake.h") // requre c++17
#if CMAKE_BUILD
#    include "config_cmake.h"
#endif

#if defined(__linux__)
#    define _linux_ 1
#elif defined(_WIN64)
#    define _win64_ 1
#    define _win32_ 1
#elif defined(__WIN32__) || defined(_WIN32)
#    define _win32_ 1
#elif defined(__APPLE__)
#    define _darwin_ 1
#    define _unix_ 1
#endif

#if defined(_win32_) || defined(_win64_)
#    define _win_ 1
#endif

#if defined(_linux_)
#    define _unix_ 1
#endif

#if defined(_MSC_VER)
#    undef NOMINMAX
#    define NOMINMAX
#    include <basetsd.h>
#    define ssize_t SSIZE_T
#    define HAVE_SSIZE_T 1
#
#    include <windows.h>
//#    include <winsock2.h>
//#    include <ws2tcpip.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

#if defined(_win_)
#    if defined(UNICODE)
#        define ODBC_WCHAR 1
#        include <sqlucode.h>

#        define strcpy wcscpy_s
#        define stricmp _wcsicmp
//#       define strncpy wcsncpy
#    else
#        define strcpy strcpy_s
#        define stricmp _stricmp
#    endif
#else

// Fix missing declarations in iodbc
#    if defined(_IODBCUNIX_H)
#        if defined(UNICODE)
#            define LPTSTR LPWSTR
#        else
#            define LPTSTR LPSTR
#        endif
#    endif
#    if defined(UNICODE)
#        define TEXT(value) L"" value
#    else
#        define TEXT(value) value
#    endif
#endif

typedef std::remove_pointer<LPTSTR>::type MYTCHAR;
#define SIZEOF_CHAR sizeof(SQLTCHAR)

#if defined(_MSC_VER) && !defined(USE_SSL)
// Enabled by default, but you can disable
#    define USE_SSL 1
#endif

#if !defined(CMAKE_SYSTEM) && _win_
#   define CMAKE_SYSTEM "windows"
#endif
