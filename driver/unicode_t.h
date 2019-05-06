#pragma once

#include <codecvt>
#include <locale>

#include "platform.h"

#if CMAKE_BUILD
#    include "config_cmake.h"
#endif

#if defined(_win_) && defined(UNICODE)
#    define ODBC_WCHAR 1
#endif

#if ODBC_WCHAR || _win_
using MY_STD_W_STRING = std::wstring;
using MY_STD_W_CHAR = wchar_t;
#else
using MY_STD_W_STRING = std::u16string;
using MY_STD_W_CHAR = char16_t;
#endif

#if defined(UNICODE)
using MYTCHAR = std::remove_pointer<LPWSTR>::type;
using MY_STD_T_STRING = MY_STD_W_STRING;
using MY_STD_T_CHAR = MY_STD_W_CHAR;
#else
using MYTCHAR = std::remove_pointer<LPSTR>::type;
using MY_STD_T_STRING = std::string;
using MY_STD_T_CHAR = char;
#endif

#if ODBC_WCHAR
using MY_UTF_W_CONVERT = std::wstring_convert<std::codecvt_utf8<MY_STD_W_CHAR>, MY_STD_T_CHAR>;
#elif defined(UNICODE)
using MY_UTF_W_CONVERT = std::wstring_convert<std::codecvt_utf8_utf16<MY_STD_W_CHAR>, MY_STD_T_CHAR>;
#endif
