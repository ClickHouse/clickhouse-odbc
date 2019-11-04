#include "driver/utils/utils.h"
#include "driver/config/config.h"
#include "driver/config/ini_defines.h"
#include "driver/driver.h"

#include <odbcinst.h>

#include <type_traits>
#include <vector>

#include <cstring>

void getDSNinfo(ConnInfo * ci, bool overwrite) {
    using CharType = std::remove_cv<std::remove_pointer<LPCTSTR>::type>::type;

    std::vector<CharType> dsn;
    std::vector<CharType> config_file;
    std::vector<CharType> name;
    std::vector<CharType> default_value;
    std::vector<CharType> value;

    fromUTF8(ci->dsn, dsn);
    fromUTF8(ODBC_INI, config_file);

#define GET_CONFIG(NAME, INI_NAME, DEFAULT)           \
    if (ci->NAME.empty() || overwrite) {              \
        fromUTF8(INI_NAME, name);                     \
        fromUTF8(DEFAULT, default_value);             \
        value.clear();                                \
        value.resize(MAX_DSN_VALUE_LEN);              \
        const auto read = SQLGetPrivateProfileString( \
            &dsn[0],                                  \
            &name[0],                                 \
            &default_value[0],                        \
            &value[0],                                \
            value.size(),                             \
            &config_file[0]                           \
        );                                            \
        if (read < 0)                                 \
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the value of " INI_NAME);        \
        if (read > MAX_DSN_VALUE_LEN)                 \
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the entire value of " INI_NAME); \
        value.resize(read);                           \
        ci->NAME = toUTF8(value);                     \
    }

	GET_CONFIG(desc,            INI_DESC,            INI_DESC_DEFAULT);
    GET_CONFIG(url,             INI_URL,             INI_URL_DEFAULT);
    GET_CONFIG(server,          INI_SERVER,          INI_SERVER_DEFAULT);
    GET_CONFIG(port,            INI_PORT,            INI_PORT_DEFAULT);
    GET_CONFIG(username,        INI_USERNAME,        INI_USERNAME_DEFAULT);
    GET_CONFIG(password,        INI_PASSWORD,        INI_PASSWORD_DEFAULT);
    GET_CONFIG(timeout,         INI_TIMEOUT,         INI_TIMEOUT_DEFAULT);
    GET_CONFIG(sslmode,         INI_SSLMODE,         INI_SSLMODE_DEFAULT);
    GET_CONFIG(database,        INI_DATABASE,        INI_DATABASE_DEFAULT);
    GET_CONFIG(onlyread,        INI_READONLY,        INI_READONLY_DEFAULT);
    GET_CONFIG(stringmaxlength, INI_STRINGMAXLENGTH, INI_STRINGMAXLENGTH_DEFAULT);
    GET_CONFIG(trace,           INI_TRACE,           INI_TRACE_DEFAULT);
    GET_CONFIG(tracefile,       INI_TRACEFILE,       INI_TRACEFILE_DEFAULT);

#undef GET_CONFIG
}

void writeDSNinfo(const ConnInfo * ci) {
    using CharType = std::remove_cv<std::remove_pointer<LPCTSTR>::type>::type;

    std::vector<CharType> dsn;
    std::vector<CharType> config_file;
    std::vector<CharType> name;
    std::vector<CharType> value;

    fromUTF8(ci->dsn, dsn);
    fromUTF8(ODBC_INI, config_file);

#define WRITE_CONFIG(NAME, INI_NAME)                      \
    {                                                     \
        fromUTF8(INI_NAME, name);                         \
        fromUTF8(ci->NAME, value);                        \
        const auto result = SQLWritePrivateProfileString( \
            &dsn[0],                                      \
            &name[0],                                     \
            &value[0],                                    \
            &config_file[0]                               \
        );                                                \
        if (!result)                                      \
            throw std::runtime_error("SQLWritePrivateProfileString failed to write value of " INI_NAME); \
    }

    WRITE_CONFIG(desc,            INI_DESC);
    WRITE_CONFIG(url,             INI_URL);
    WRITE_CONFIG(server,          INI_SERVER);
    WRITE_CONFIG(port,            INI_PORT);
    WRITE_CONFIG(username,        INI_USERNAME);
    WRITE_CONFIG(password,        INI_PASSWORD);
    WRITE_CONFIG(timeout,         INI_TIMEOUT);
    WRITE_CONFIG(sslmode,         INI_SSLMODE);
    WRITE_CONFIG(database,        INI_DATABASE);
    WRITE_CONFIG(onlyread,        INI_READONLY);
    WRITE_CONFIG(stringmaxlength, INI_STRINGMAXLENGTH);
    WRITE_CONFIG(trace,           INI_TRACE);
    WRITE_CONFIG(tracefile,       INI_TRACEFILE);

#undef WRITE_CONFIG
}
