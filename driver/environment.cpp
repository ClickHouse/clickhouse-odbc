#include "environment.h"
#include "win/version.h"

#include <cstdio>
#include <ctime>
#include <string>

#if defined (_unix_)
#   include <unistd.h>
#   include <pwd.h>
#endif

//#if __has_include("config_cmake.h") // requre c++17
#if CMAKE_BUILD
#   include "config_cmake.h"
#endif

const std::map<std::string, TypeInfo> Environment::types_info =
{
    { "UInt8",       TypeInfo{ "TINYINT",   true,    SQL_TINYINT,         3,  1 } },
    { "UInt16",      TypeInfo{ "SMALLINT",  true,    SQL_SMALLINT,        5,  2 } },
    { "UInt32",      TypeInfo{ "INT",       true,    SQL_BIGINT /* was SQL_INTEGER */,         10, 4 } }, // With perl, python ODBC drivers INT is uint32 and it cant store values bigger than 2147483647: 2147483648 -> -2147483648 4294967295 -> -1
    { "UInt32",      TypeInfo{ "INT",       true,    SQL_INTEGER,         10, 4 } },
    { "UInt64",      TypeInfo{ "BIGINT",    true,    SQL_BIGINT,          19, 8 } },
    { "Int8",        TypeInfo{ "TINYINT",   false,   SQL_TINYINT,         3,  1 } },
    { "Int16",       TypeInfo{ "SMALLINT",  false,   SQL_SMALLINT,        5,  2 } },
    { "Int32",       TypeInfo{ "INT",       false,   SQL_INTEGER,         10, 4 } },
    { "Int64",       TypeInfo{ "BIGINT",    false,   SQL_BIGINT,          20, 8 } },
    { "Float32",     TypeInfo{ "REAL",      false,   SQL_REAL,            7,  4 } },
    { "Float64",     TypeInfo{ "DOUBLE",    false,   SQL_DOUBLE,          15, 8 } },
    { "String",      TypeInfo{ "TEXT",      true,    SQL_VARCHAR,         0xFFFFFF, (1 << 20) } },
    { "FixedString", TypeInfo{ "TEXT",      true,    SQL_VARCHAR,         0xFFFFFF, (1 << 20) } },
    { "Date",        TypeInfo{ "DATE",      true,    SQL_TYPE_DATE,       10, 6 } },
    { "DateTime",    TypeInfo{ "TIMESTAMP", true,    SQL_TYPE_TIMESTAMP,  19, 16 } },
    { "Array",       TypeInfo{ "TEXT",      true,    SQL_VARCHAR,         0xFFFFFF, (1 << 20) } },
};

Environment::Environment() {
#if OUTPUT_REDIRECT
    std::string stderr_path = "/tmp/clickhouse-odbc-stderr";
#   if _unix_
    struct passwd * pw;
    uid_t uid;
    uid = geteuid();
    pw = getpwuid(uid);
    if (pw)
        stderr_path += "." + std::string(pw->pw_name);
#   endif

#   if _win_
    // unsigned int pid = GetCurrentProcessId();
    // stderr_path += "." + std::to_string(pid);
#   endif

    if (!freopen(stderr_path.c_str(), "a", stderr))
        throw std::logic_error("Cannot freopen stderr.");

#endif
    {
        auto t = std::time(nullptr);
        char mbstr[100];
        if (std::strftime(mbstr, sizeof(mbstr), "%Y.%m.%d %T", std::localtime(&t))) {
#if OUTPUT_REDIRECT
            std::cerr << mbstr << " === Driver started =====================" << std::endl;
#endif
            LOG(std::endl << mbstr << " === Driver started ==="
                      << " VERSION=" << VERSION_STRING
#if !defined(_MSC_VER) // TODO: wtf with preprocessor here?

#if defined(UNICODE)
                      << " UNICODE=" << UNICODE
#   if defined(ODBC_WCHAR)
                      << " ODBC_WCHAR=" << ODBC_WCHAR
#   endif
                      << " sizeof(SQLTCHAR)=" << sizeof(SQLTCHAR)
                      << " sizeof(wchar_t)=" << sizeof(wchar_t)
#endif
#if ODBCVER
                      << " ODBCVER=" << std::hex << ODBCVER << std::dec
#endif
#if defined(ODBC_LIBRARIES)
                      << " ODBC_LIBRARIES=" << ODBC_LIBRARIES
#endif
#if defined(ODBC_INCLUDE_DIRECTORIES)
                      << " ODBC_INCLUDE_DIRECTORIES=" << ODBC_INCLUDE_DIRECTORIES
#endif
#endif
            );
        }
    }
}

Environment::~Environment() {
    LOG("========== ======== Driver stopped =====================");
}
