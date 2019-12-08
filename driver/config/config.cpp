#include "driver/utils/utils.h"
#include "driver/config/config.h"
#include "driver/config/ini_defines.h"
#include "driver/driver.h"
#include "driver/environment.h"

#include <odbcinst.h>

#include <list>
#include <string>

#include <cstring>

bool DSNExists(Environment & env, const std::string & target_dsn) {
    // First, rewind to the end.
    while (true) {
        const auto rc = SQLDataSources(
            env.getHandle(),
            SQL_FETCH_NEXT,
            nullptr,
            MAX_DSN_VALUE_LEN,
            nullptr,
            nullptr,
            MAX_DSN_VALUE_LEN,
            nullptr
        );

        if (rc == SQL_NO_DATA)
            break;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("failed to iterate over defined data sources");
    }

    // Now, rewind from the start to the end, comparing each next DSN to the target.
    while (true) {
        std::basic_string<SQLTCHAR> dsn(MAX_DSN_VALUE_LEN, SQLTCHAR{});
        SQLSMALLINT dsn_len = 0;

        const auto rc = SQLDataSources(
            env.getHandle(),
            SQL_FETCH_NEXT,
            const_cast<SQLTCHAR *>(dsn.data()),
            dsn.size(),
            &dsn_len,
            nullptr,
            MAX_DSN_VALUE_LEN,
            nullptr
        );

        if (rc == SQL_NO_DATA)
            break;

        if (!SQL_SUCCEEDED(rc))
            throw std::runtime_error("failed to iterate over defined data sources");

        if (dsn_len < 0 || dsn_len >= dsn.size())
            throw std::runtime_error("failed to extract data source name");

        dsn.resize(dsn_len);

        if (Poco::UTF8::icompare(target_dsn, toUTF8(dsn)) == 0)
            return true;
    }

    return false;
}

void readDSNinfo(ConnInfo * ci, bool overwrite) {
    std::basic_string<CharTypeLPCTSTR> dsn;
    std::basic_string<CharTypeLPCTSTR> config_file;
    std::basic_string<CharTypeLPCTSTR> name;
    std::basic_string<CharTypeLPCTSTR> default_value;
    std::basic_string<CharTypeLPCTSTR> value;

    fromUTF8(ci->dsn, dsn);
    fromUTF8(ODBC_INI, config_file);

#define GET_CONFIG(NAME, INI_NAME, DEFAULT)              \
    if (ci->NAME.empty() || overwrite) {                 \
        fromUTF8(INI_NAME, name);                        \
        fromUTF8(DEFAULT, default_value);                \
        value.clear();                                   \
        value.resize(MAX_DSN_VALUE_LEN);                 \
        const auto read = SQLGetPrivateProfileString(    \
            dsn.c_str(),                                 \
            name.c_str(),                                \
            default_value.c_str(),                       \
            const_cast<CharTypeLPCTSTR *>(value.data()), \
            value.size(),                                \
            config_file.c_str()                          \
        );                                               \
        if (read < 0)                                    \
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the value of " INI_NAME);        \
        if (read >= value.size())                        \
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the entire value of " INI_NAME); \
        value.resize(read);                              \
        ci->NAME = toUTF8(value);                        \
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
    GET_CONFIG(driverlog,       INI_DRIVERLOG,       INI_DRIVERLOG_DEFAULT);
    GET_CONFIG(driverlogfile,   INI_DRIVERLOGFILE,   INI_DRIVERLOGFILE_DEFAULT);

#undef GET_CONFIG
}

void writeDSNinfo(const ConnInfo * ci) {
    std::basic_string<CharTypeLPCTSTR> dsn;
    std::basic_string<CharTypeLPCTSTR> config_file;
    std::basic_string<CharTypeLPCTSTR> name;
    std::basic_string<CharTypeLPCTSTR> value;

    fromUTF8(ci->dsn, dsn);
    fromUTF8(ODBC_INI, config_file);

#define WRITE_CONFIG(NAME, INI_NAME)                      \
    {                                                     \
        fromUTF8(INI_NAME, name);                         \
        fromUTF8(ci->NAME, value);                        \
        const auto result = SQLWritePrivateProfileString( \
            dsn.c_str(),                                  \
            name.c_str(),                                 \
            value.c_str(),                                \
            config_file.c_str()                           \
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
    WRITE_CONFIG(driverlog,       INI_DRIVERLOG);
    WRITE_CONFIG(driverlogfile,   INI_DRIVERLOGFILE);

#undef WRITE_CONFIG
}

key_value_map_t readConnectionString(const std::string & connection_string) {
    key_value_map_t fields;

    std::string cs = connection_string + ';';

    const auto extract_key_name = [] (std::string & str) {
        std::string key;

        while (!str.empty() && (std::isspace(str.front()) || str.front() == ';')) {
            str.erase(0, 1);
        }

        bool equal_met = false;

        while (!str.empty()) {
            if (str.front() == ';') {
                break;
            }
            else if (str.front() == '=') {
                if (str.size() > 1 && str[1] == '=') {
                    key += '=';
                    str.erase(0, 2);
                }
                else {
                    str.erase(0, 1);
                    equal_met = true;
                    break;
                }
            }
            else {
                key += str.front();
                str.erase(0, 1);
            }
        }

        while (!key.empty() && std::isspace(key.back())) {
            key.pop_back();
        }

        if (key.empty())
            throw std::runtime_error("key name is missing");

        if (!equal_met)
            throw std::runtime_error("'=' is missing");

        return key;
    };

    const auto extract_key_value = [] (std::string & str) {
        std::string value;

        while (!str.empty() && std::isspace(str.front())) {
            str.erase(0, 1);
        }

        bool stop_at_closing_brace = false;
        bool stopped_at_closing_brace = false;

        if (!str.empty() && str.front() == '{') {
            stop_at_closing_brace = true;
            str.erase(0, 1);
        }

        while (!str.empty()) {
            if (stop_at_closing_brace && str.front() == '}') {
                stopped_at_closing_brace = true;
                str.erase(0, 1);
                break;
            }
            else if (!stop_at_closing_brace && str.front() == ';') {
                stopped_at_closing_brace = false;
                str.erase(0, 1);
                break;
            }
            else {
                value += str.front();
                str.erase(0, 1);
            }
        }

        if (stop_at_closing_brace) {
            if (!stopped_at_closing_brace)
                throw std::runtime_error("'}' expected");
        }
        else {
            while (!value.empty() && std::isspace(value.back())) {
                value.pop_back();
            }
        }

        return value;
    };

    while (!cs.empty()) {
        const auto key = extract_key_name(cs);
        const auto value = extract_key_value(cs);

        const auto key_upper = Poco::UTF8::toUpper(key);

        if (
            fields.find(key) != fields.end() ||
            (key_upper == INI_DSN && fields.find(INI_FILEDSN) != fields.end()) ||
            (key_upper == INI_FILEDSN && fields.find(INI_DSN) != fields.end()) ||
            (key_upper == INI_DSN && fields.find(INI_DRIVER) != fields.end()) ||
            (key_upper == INI_DRIVER && fields.find(INI_DSN) != fields.end())
        ) {
            break; // These doesn't override each other: the first one met is used.
        }

        fields[key] = value;
    }

    return fields;
}

key_value_map_t readDSNInfo(const std::string & dsn_utf8) {
    key_value_map_t fields;

    std::basic_string<CharTypeLPCTSTR> dsn;
    std::basic_string<CharTypeLPCTSTR> config_file;
    std::basic_string<CharTypeLPCTSTR> default_value; // leave this empty
    std::list<std::basic_string<CharTypeLPCTSTR>> keys;

    fromUTF8(dsn_utf8, dsn);
    fromUTF8(ODBC_INI, config_file);

    // Read all key names under the specified DSN.
    {
        std::basic_string<CharTypeLPCTSTR> keys_all(MAX_DSN_KEY_LEN * 1000, CharTypeLPCTSTR{});

        const auto read = SQLGetPrivateProfileString(
            dsn.c_str(),
            nullptr,
            default_value.c_str(),
            const_cast<CharTypeLPCTSTR *>(keys_all.data()),
            keys_all.size(),
            config_file.c_str()
        );

        if (read < 0)
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the key list of DSN " + dsn_utf8);

        if (read >= keys_all.size())
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the entire key list of DSN " + dsn_utf8);
        
        keys_all.resize(read);

        std::basic_string<CharTypeLPCTSTR> key;
        for (auto ch : keys_all) {
            if (ch == CharTypeLPCTSTR{}) {
                if (!key.empty()) {
                    keys.push_back(key);
                    key.clear();
                }
            }
            else {
                key += ch;
            }
        }

        if (!key.empty())
            keys.push_back(key);
    }

    // Read values of each key.
    for (const auto key : keys) {
        const auto key_utf8 = toUTF8(key);

        if (fields.find(key_utf8) != fields.end()) {
            LOG("Repeating key " << key_utf8 << " in DSN " << dsn_utf8 << ", ignoring, the first met value will be used");
            break;
        }

        if (
            Poco::UTF8::icompare(key_utf8, INI_DSN) == 0 ||
            Poco::UTF8::icompare(key_utf8, INI_DRIVER) == 0 ||
            Poco::UTF8::icompare(key_utf8, INI_FILEDSN) == 0 ||
            Poco::UTF8::icompare(key_utf8, INI_SAVEFILE) == 0
        ) {
            LOG("Unexpected key " << key_utf8 << " in DSN " << dsn_utf8 << ", ignoring");
            break;
        }

        std::basic_string<CharTypeLPCTSTR> value(MAX_DSN_VALUE_LEN, CharTypeLPCTSTR{});

        const auto read = SQLGetPrivateProfileString(
            dsn.c_str(),
            key.c_str(),
            default_value.c_str(),
            const_cast<CharTypeLPCTSTR *>(value.data()),
            value.size(),
            config_file.c_str()
        );

        if (read < 0)
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the value of " + key_utf8);

        if (read >= value.size())
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the entire value of " + key_utf8);

        value.resize(read);

        fields[key_utf8] = toUTF8(value);
    }

    return fields;
}
