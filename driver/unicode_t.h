#pragma once

#include <codecvt>
#include <locale>

#if CMAKE_BUILD
#    include "config_cmake.h"
#endif

#if defined(UNICODE)
using MYTCHAR = std::remove_pointer<LPWSTR>::type;
#    if ODBC_WCHAR
using MY_STD_T_STRING = std::wstring;
using MY_STD_T_CHAR = wchar_t;
#    else
using MY_STD_T_STRING = std::u16string;
using MY_STD_T_CHAR = char16_t;
#    endif

#    if ODBC_WCHAR
using MY_UTF_T_CONVERT = std::wstring_convert<std::codecvt_utf8<MY_STD_T_CHAR>, MY_STD_T_CHAR>;
#    else
using MY_UTF_T_CONVERT = std::wstring_convert<std::codecvt_utf8_utf16<MY_STD_T_CHAR>, MY_STD_T_CHAR>;
#    endif

#else
using MYTCHAR = std::remove_pointer<LPSTR>::type;
using MY_STD_T_STRING = std::string;
using MY_STD_T_CHAR = char;
#endif
