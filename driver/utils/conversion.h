#pragma once

#include "driver/platform/platform.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <cstring>

/***********************************************************************************************************************
// All ODBC functions expect strings to be passed as pointers to unsigned character types:
// either `unsigned char` for ANSI functions or `unsigned short` for Unicode functions.
// This is inconvenient when writing C++ code, as we typically use std::string and std::u16string.
// Using std::basic_string<unsigned char> or std::basic_string<unsigned short> is both verbose and generally
// discouraged. For example, libc++ removed `std::char_traits` specializations for such character types.
//
// Therefore, it is easier to use standard C++ string types and cast from signed character pointers (e.g., char*) to
// unsigned ones (e.g., SQLCHAR*) only when calling ODBC functions.
//
// The purpose of the code below is to define a universal approach for creating and passing strings to ODBC functions:
//
//     // 1. Create a string, either `std::string` or `std::u16string`, depending on whether UNICODE is defined
//     auto query = fromUTF8<PTChar>("SELECT VERSION();");
//
//     // 2. Call an ODBC function, passing a pointer to the data converted to the expected type
//     SQLExecDirect(stmt, asSqlTChar(query.data()), SQL_NTS);
//
***********************************************************************************************************************/

// PTChar - Portable Text Char.
// It is similar to SQLTCHAR (SQL Text CHARacter), but instead of using unsigned types, which are not well-supported
// by the C++ standard, it is defined in terms of well-defined signed types.
#ifdef UNICODE
    using PTChar = char16_t;   // then std::basic_string<PTChar> is the same as std::u16string
#else
    using PTChar = char;       // then std::basic_string<PTChar> is the same as std::string
#endif

// Map each signed char types with ODBC's unsigned char types
template <typename C>
struct AsSqlCharTextConversionTrait
{
};

template <>
struct AsSqlCharTextConversionTrait<char>
{
    using type = SQLCHAR;
};

template <>
struct AsSqlCharTextConversionTrait<char16_t>
{
    using type = SQLWCHAR;
};

// Cast C++ string character pointer types (i.e. char * and char16_t *) to ODBC's unsigned pointer types,
// i.e. SQLCHAR* and SQLTCHAR*. In essence it is just a reinterpret_cast, however it prevents incorrect conversions
// and does not allow conversions to arbitrary types.
// In short, char* will always be converted to SQLCHAR*, and char16_t* will always be converted to SQLWCHAR*.
template <typename C>
constexpr auto ptcharCast(C * str)
{
    // Make sure that const and volatile are preserved
    using T = typename AsSqlCharTextConversionTrait<std::remove_cv_t<C>>::type;
    using VolatileT = typename std::conditional_t<std::is_volatile_v<C>, volatile T, T>;
    using ConstVolatileT = typename std::conditional_t<std::is_const_v<C>, const VolatileT, VolatileT>;

    static_assert(sizeof(T) == sizeof(C));
    return reinterpret_cast<ConstVolatileT *>(str);
}

// The situation is complicated for Windows string types, such as LPSTR, LPCSTR, LPWSTR, etc.
// For comparison, ODBC types, such as SQLCHAR, SQLWCHAR, are always unsigned, however
// Windows' LPSTR, LPWSTR, etc. do not have this property:
//  - LPSTR, aka Long Pointer STRing is a `char*`;
//  - LPWSTR, aka Long Pointer Wide STRing, is `wchar_t *` on windows and `unsigned short*` on unixODBC.
//  - The LPTSTR alias, aka Long Pointer Text STRing, is `unsigned short*` when UNICODE is defined
//    otherwise `unsigned char*` -- that is different than LPSTR and LPWSTR.
// We need a portable type to which we can convert the content of our `std::basic_string<PTChar>`
// when calling Windows functions, such as SQLGetPrivateProfileString, SQLWriteDSNToIni, etc.
// That is what WinTChar for. The idea is to use `std::basic_string<PTChar>` and then cast its content
// to `WinTChar*` when calling such Windows functions.
#ifdef UNICODE
    using  WinTChar = std::remove_pointer_t<LPWSTR>;
#else
    using WinTChar = std::remove_pointer_t<LPSTR>;
#endif

// A slightly safer version of reinterpret_cast that prevents casting pointers to pointers of a different size.
// This prevents from incorrectly converting string pointers, for example std::string::data() to `unsigned short*`, when
// calling an ODBC function.
template <typename Out, typename In>
constexpr Out* arrayPtrCast(In * in)
{
    static_assert(sizeof(In) == sizeof(Out));
    return reinterpret_cast<Out*>(in);
}

/***********************************************************************************************************************
// UTF Conversion Functions
***********************************************************************************************************************/

// stringBufferLength() - return the number of elements in the null-terminated buffer (that is assumed to hold a string).

template <typename CharType>
inline std::size_t stringBufferLength(const CharType * str) {
    return (str ? std::basic_string_view<CharType>{str}.size() + 1 : 0);
}

template <>
inline std::size_t stringBufferLength<SQLTCHAR>(const SQLTCHAR * str) {
    static_assert(sizeof(SQLTCHAR) == sizeof(PTChar)); // Helps noticing stupid refactoring mistakes
    return stringBufferLength(reinterpret_cast<const PTChar*>(str));
}

template <typename CharType>
inline std::size_t stringLengthUTF8(const std::basic_string<CharType> & str) {
    return stringLengthUTF8(make_string_view(str));
}

template <typename CharType>
inline std::size_t stringLengthUTF8(const CharType * str, const std::size_t size) {
    if (!str || !size)
        return 0;

    return stringLengthUTF8(std::basic_string_view<CharType>{str, size});
}

template <typename CharType>
inline std::size_t stringLengthUTF8(const CharType * str) {
    return stringLengthUTF8(str, stringBufferLength(str));
}


#if defined(WORKAROUND_USE_ICU)
#   include "driver/utils/conversion_icu.h"
#else
#   include "driver/utils/conversion_std.h"
#endif
