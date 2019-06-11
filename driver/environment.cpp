#include "environment.h"

//#if __has_include("config_cmake.h") // requre c++17
#if CMAKE_BUILD
#    include "config_cmake.h"
#endif

#include <cstdio>
#include <ctime>
#include <sstream>
#include <string>
#include "win/version.h"
#include "unicode_t.h"

#if defined(_unix_)
#    include <pwd.h>
#    include <unistd.h>
#endif


const std::map<std::string, TypeInfo> Environment::types_info = {
    {"UInt8", TypeInfo {"TINYINT", true, SQL_TINYINT, 3, 1}},
    {"UInt16", TypeInfo {"SMALLINT", true, SQL_SMALLINT, 5, 2}},
    {"UInt32",
        TypeInfo {"INT",
            true,
            SQL_BIGINT /* was SQL_INTEGER */,
            10,
            4}}, // With perl, python ODBC drivers INT is uint32 and it cant store values bigger than 2147483647: 2147483648 -> -2147483648 4294967295 -> -1
    {"UInt32", TypeInfo {"INT", true, SQL_INTEGER, 10, 4}},
    {"UInt64", TypeInfo {"BIGINT", true, SQL_BIGINT, 20, 8}},
    {"Int8", TypeInfo {"TINYINT", false, SQL_TINYINT, 1 + 3, 1}}, // one char for sign
    {"Int16", TypeInfo {"SMALLINT", false, SQL_SMALLINT, 1 + 5, 2}},
    {"Int32", TypeInfo {"INT", false, SQL_INTEGER, 1 + 10, 4}},
    {"Int64", TypeInfo {"BIGINT", false, SQL_BIGINT, 1 + 19, 8}},
    {"Float32", TypeInfo {"REAL", false, SQL_REAL, 7, 4}},
    {"Float64", TypeInfo {"DOUBLE", false, SQL_DOUBLE, 15, 8}},
    {"Decimal", TypeInfo {"DECIMAL", false, SQL_DECIMAL, 1 + 2 + 38, 16}}, // -0.
    {"String", TypeInfo {"TEXT", true, SQL_VARCHAR, Environment::string_max_size, Environment::string_max_size}},
    {"FixedString", TypeInfo {"TEXT", true, SQL_VARCHAR, Environment::string_max_size, Environment::string_max_size}},
    {"Date", TypeInfo {"DATE", true, SQL_TYPE_DATE, 10, 6}},
    {"DateTime", TypeInfo {"TIMESTAMP", true, SQL_TYPE_TIMESTAMP, 19, 16}},
    {"Array", TypeInfo {"TEXT", true, SQL_VARCHAR, Environment::string_max_size, Environment::string_max_size}},

    {"LowCardinality(String)",
        TypeInfo {"TEXT", true, SQL_VARCHAR, Environment::string_max_size, Environment::string_max_size}}, // todo: remove
    {"LowCardinality(FixedString)",
        TypeInfo {"TEXT", true, SQL_VARCHAR, Environment::string_max_size, Environment::string_max_size}}, // todo: remove
};

Environment::Environment() {
#if OUTPUT_REDIRECT
    std::string stderr_path = "/tmp/clickhouse-odbc-stderr";
#    if _unix_
    struct passwd * pw;
    uid_t uid;
    uid = geteuid();
    pw = getpwuid(uid);
    if (pw)
        stderr_path += "." + std::string(pw->pw_name);
#    endif

#    if _win_
        // unsigned int pid = GetCurrentProcessId();
        // stderr_path += "." + std::to_string(pid);
#    endif

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
            LOG(std::endl << mbstr);
        }

        log_header = " === Driver started ===";
        log_header += " VERSION=" + std::string {VERSION_STRING};
#if defined(_win64_)
        log_header += " WIN64";
#elif defined(_win32_)
        log_header += " WIN32";
#endif
#if ODBC_IODBC
        log_header += " ODBC_IODBC";
#endif
#if ODBC_CHAR16
        log_header += " ODBC_CHAR16";
#endif
#if ODBC_UNIXODBC
        log_header += " ODBC_UNIXODBC";
#endif

#if defined(UNICODE)
        log_header += " UNICODE=" + std::to_string(UNICODE);
#    if defined(ODBC_WCHAR)
        log_header += " ODBC_WCHAR=" + std::to_string(ODBC_WCHAR);
#    endif
        log_header += " sizeof(SQLTCHAR)=" + std::to_string(sizeof(SQLTCHAR)) + " sizeof(wchar_t)=" + std::to_string(sizeof(wchar_t));
#endif
#if defined(SQL_WCHART_CONVERT)
        log_header += " SQL_WCHART_CONVERT";
#endif
#if ODBCVER
        std::stringstream strm;
        strm << " ODBCVER=" << std::hex << ODBCVER << std::dec;
        log_header += strm.str();
#endif
#if defined(ODBC_LIBRARIES)
        log_header += " ODBC_LIBRARIES=" + std::string {ODBC_LIBRARIES};
#endif
#if defined(ODBC_INCLUDE_DIRECTORIES)
        log_header += " ODBC_INCLUDE_DIRECTORIES=" + std::string {ODBC_INCLUDE_DIRECTORIES};
#endif

        if (log_stream.is_open()) {
            LOG(log_header);
            log_header.clear();
        }
    }
}

Environment::~Environment() {
    LOG("========== ======== Driver stopped =====================");
}


const TypeInfo & Environment::getTypeInfo(const std::string & type_name, const std::string & type_name_without_parametrs) const {
    if (types_info.find(type_name) != types_info.end())
        return types_info.at(type_name);
    if (types_info.find(type_name_without_parametrs) != types_info.end())
        return types_info.at(type_name_without_parametrs);
    LOG("Unsupported type " << type_name << " : " << type_name_without_parametrs);
    throw SqlException("Unsupported type = " + type_name, "HY004");
}
