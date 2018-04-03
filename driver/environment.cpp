#include "environment.h"

#if defined (_unix_)
#   include <stdio.h>
#   include <unistd.h>
#   include <pwd.h>

//#if __has_include("config_cmake.h") // requre c++17
#if CMAKE_BUILD
#include "config_cmake.h"
#endif
#endif

const std::map<std::string, TypeInfo> Environment::types_info =
{
    { "UInt8",       TypeInfo{ "TINYINT",   true,    SQL_TINYINT,         3,  1 } },
    { "UInt16",      TypeInfo{ "SMALLINT",  true,    SQL_SMALLINT,        5,  2 } },
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

Environment::Environment()
{
#if defined (_unix_)
#if !NO_OUTPUT_REDIRECT
    struct passwd *pw;
    uid_t uid;
    std::string stderr_path = "/tmp/clickhouse-odbc-stderr";
    uid = geteuid();
    pw = getpwuid(uid);
    if (pw)
    {
        stderr_path += "." + std::string(pw->pw_name);
    }
    if (!freopen(stderr_path.c_str(), "w", stderr))
        throw std::logic_error("Cannot freopen stderr.");
#endif
#endif
}

Environment::~Environment()
{ }