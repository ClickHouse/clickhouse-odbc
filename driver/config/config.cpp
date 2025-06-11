#include "driver/utils/utils.h"
#include "driver/config/config.h"
#include "driver/config/ini_defines.h"
#include "driver/driver.h"
#include "driver/environment.h"

#include <odbcinst.h>

#include <list>
#include <string>

#include <cctype>
#include <cstring>

// Parses connection string in a form of "key1=value1;key2={value 2};..." into a key->value map.
// Expects a syntax of connection string as defined in:
//     https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqldriverconnect-function?view=sqlallproducts-allversions#comments
//     (however understands somewhat relaxed and more generic version of that syntax)
// In short:
//     - any syntactically correct key is allowed at this point, semantic filltering is done elsewhere
//     - keys cannot be empty
//     - keys are case-insensitive
//     - '=' in the key itself should be written as '==' (whithout space in between)
//     - whitespace aroung keys and values is trimmed
//     - values can be enclosed in '{' and '}', in which case the whitespace at the beginning and/or
//       at the end of the value, as well as ';' characters anywhere in the value, will be preserved as-is
//     - if key is met multiple times, the first value will be used
//     - some special cases that modify the value visibility behavior for some well-known keys are defined (see the link above)
key_value_map_t readConnectionString(const std::string & connection_string) {
    key_value_map_t fields;

    std::string cs = connection_string;

    const auto consume_space = [] (std::string & str) {
        std::size_t num = 0;
        while (!str.empty() && std::isspace(static_cast<unsigned char>(str[num]))) {
            ++num;
        }
        str.erase(0, num);
        return num;
    };

    const auto consume_semi_colons = [] (std::string & str) {
        std::size_t num = 0;
        while (!str.empty() && str[num] == ';') {
            ++num;
        }
        str.erase(0, num);
        return num;
    };

    const auto extract_key_name = [&consume_space, &consume_semi_colons] (std::string & str) {
        std::string key;

        while (consume_space(str) || consume_semi_colons(str));

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

        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) {
            key.pop_back();
        }

        if (key.empty())
            throw std::runtime_error("key name is missing");

        if (!equal_met)
            throw std::runtime_error("'=' is missing");

        return key;
    };

    const auto extract_key_value = [&consume_space, &consume_semi_colons] (std::string & str) {
        std::string value;

        while (consume_space(str));

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
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
                value.pop_back();
            }
        }

        std::size_t consumed = 0;
        std::size_t consumed_semi_colons = 0;

        do {
            consumed = consume_semi_colons(str);
            consumed_semi_colons += consumed;
            consumed += consume_space(str);
        } while (consumed > 0);

        if (consumed_semi_colons == 0 && !str.empty())
            throw std::runtime_error("';' expected");

        return value;
    };

    while (true) {
        while (consume_space(cs) || consume_semi_colons(cs));

        if (cs.empty())
            break;

        const auto key = extract_key_name(cs);
        const auto value = extract_key_value(cs);

        if (
            fields.find(key) != fields.end() ||
            (Poco::UTF8::icompare(key, INI_DSN) == 0 && fields.find(INI_FILEDSN) != fields.end()) ||
            (Poco::UTF8::icompare(key, INI_FILEDSN) == 0 && fields.find(INI_DSN) != fields.end()) ||
            (Poco::UTF8::icompare(key, INI_DSN) == 0 && fields.find(INI_DRIVER) != fields.end()) ||
            (Poco::UTF8::icompare(key, INI_DRIVER) == 0 && fields.find(INI_DSN) != fields.end())
        ) {
            continue; // These doesn't override each other: the first one met is used.
        }

        fields[key] = value;
    }

    return fields;
}

// Extracts configuration info from a DSN entry into a key->value map.
// Note, that for duplicate keys in the DSN, the first value is used.
// This function does its best on detecting and reporting such duplicates in the log.
// Due to the nature of the ODBC API, a special value of '__default__' has to be reserved by this function,
// and used in such a manner, that if some key has it, that key will be treated as non-existent.
key_value_map_t readDSNInfo(const std::string & dsn_utf8) {
    key_value_map_t fields;

    std::basic_string<PTChar> dsn;
    std::basic_string<PTChar> config_file;
    const auto default_value = fromUTF8<PTChar>("__default__"); // Assuming, __default__ will never be used as a value for any key.
    std::list<std::basic_string<PTChar>> keys;

    fromUTF8(dsn_utf8, dsn);
    fromUTF8(ODBC_INI, config_file);

    // Read all key names under the specified DSN.
    {
        std::basic_string<PTChar> keys_all(MAX_DSN_KEY_LEN * 1000, PTChar{});

        const auto read = SQLGetPrivateProfileString(
            arrayPtrCast<const WinTChar>(dsn.c_str()),
            nullptr,
            arrayPtrCast<const WinTChar>(default_value.c_str()),
            arrayPtrCast<WinTChar>(keys_all.data()),
            keys_all.size(),
            arrayPtrCast<const WinTChar>(config_file.c_str())
        );

        if (read < 0)
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the key list of DSN " + dsn_utf8);

        if (read >= keys_all.size())
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the entire key list of DSN " + dsn_utf8);

        keys_all.resize(read);

        std::basic_string<PTChar> key;
        for (auto ch : keys_all) {
            if (ch == WinTChar{}) {
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

// BEGIN WORKAROUND: some versions of UnixODBC instead of returning all keys return only the first one,
// so here we make sure that all known keys will aslo be tried.
    for (const auto & known_key :
        {
            INI_DRIVER,
            INI_FILEDSN,
            INI_SAVEFILE,
            INI_DSN,
            INI_DESC,
            INI_URL,
            INI_UID,
            INI_USERNAME,
            INI_PWD,
            INI_PASSWORD,
            INI_PROTO,
            INI_SERVER,
            INI_HOST,
            INI_PORT,
            INI_TIMEOUT,
            INI_VERIFY_CONNECTION_EARLY,
            INI_SSLMODE,
            INI_PRIVATEKEYFILE,
            INI_CERTIFICATEFILE,
            INI_CALOCATION,
            INI_PATH,
            INI_DATABASE,
            INI_STRINGMAXLENGTH,
            INI_DRIVERLOG,
            INI_DRIVERLOGFILE,
            INI_AUTO_SESSION_ID
        }
    ) {
        if (
            std::find_if(
                keys.begin(),
                keys.end(),
                [&] (const auto & key) { return (Poco::UTF8::icompare(known_key, toUTF8(key)) == 0); }
            ) == keys.end()
        ) {
            keys.push_back(fromUTF8<PTChar>(known_key));
        }
    }
// END WORKAROUND

    // Read values of each key.
    for (const auto key : keys) {
        const auto key_utf8 = toUTF8(key);

        if (fields.find(key_utf8) != fields.end()) {
            LOG("Repeating key " << key_utf8 << " in DSN " << dsn_utf8 << ", ignoring, the first met value will be used");
            continue;
        }

        std::basic_string<PTChar> value(MAX_DSN_VALUE_LEN, WinTChar{});

        const auto read = SQLGetPrivateProfileString(
            arrayPtrCast<const WinTChar>(dsn.c_str()),
            arrayPtrCast<const WinTChar>(key.c_str()),
            arrayPtrCast<const WinTChar>(default_value.c_str()),
            arrayPtrCast<WinTChar>(value.data()),
            value.size(),
            arrayPtrCast<const WinTChar>(config_file.c_str())
        );

        if (read < 0)
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the value of " + key_utf8);

        if (read >= value.size())
            throw std::runtime_error("SQLGetPrivateProfileString failed to extract the entire value of " + key_utf8);

        value.resize(read);

        if (value != default_value)
            fields[key_utf8] = toUTF8(value);
    }

    return fields;
}
