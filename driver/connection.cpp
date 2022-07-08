#include "driver/utils/utils.h"
#include "driver/config/ini_defines.h"
#include "driver/connection.h"
#include "driver/descriptor.h"
#include "driver/statement.h"

#include <Poco/Base64Encoder.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/NumberParser.h> // TODO: switch to std
#include <Poco/URI.h>
#include <random>

#if !defined(WORKAROUND_DISABLE_SSL)
#    include <Poco/Net/AcceptCertificateHandler.h>
#    include <Poco/Net/RejectCertificateHandler.h>

#    include <Poco/Net/HTTPSClientSession.h>
#    include <Poco/Net/InvalidCertificateHandler.h>
#    include <Poco/Net/PrivateKeyPassphraseHandler.h>
#    include <Poco/Net/SSLManager.h>
#endif

std::once_flag ssl_init_once;

#if !defined(WORKAROUND_DISABLE_SSL)
void SSLInit(bool ssl_strict, const std::string & privateKeyFile, const std::string & certificateFile, const std::string & caLocation) {
// http://stackoverflow.com/questions/18315472/https-request-in-c-using-poco
    Poco::Net::initializeSSL();
    Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> ptrHandler;
    if (ssl_strict)
        ptrHandler = new Poco::Net::RejectCertificateHandler(false);
    else
        ptrHandler = new Poco::Net::AcceptCertificateHandler(false);
    Poco::Net::Context::Ptr ptrContext = new Poco::Net::Context(Poco::Net::Context::CLIENT_USE,
        privateKeyFile
#    if !defined(SECURITY_WIN32)
        // Do not work with poco/NetSSL_Win:
        ,
        certificateFile,
        caLocation,
        ssl_strict ? Poco::Net::Context::VERIFY_STRICT : Poco::Net::Context::VERIFY_RELAXED,
        9,
        true,
        "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"
#    endif
    );
    Poco::Net::SSLManager::instance().initializeClient(0, ptrHandler, ptrContext);
}
#endif

std::string GenerateSessionId() {
    std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<std::uint64_t> distribution(0);
    return std::string("clickhouse_odbc_" + std::to_string(distribution(generator)));
}

Connection::Connection(Environment & environment)
    : ChildType(environment),
      session_id(GenerateSessionId())
{
    resetConfiguration();
}

const TypeInfo & Connection::getTypeInfo(const std::string & type_name, const std::string & type_name_without_parameters) const {
    auto tmp_type_name = type_name;
    auto tmp_type_name_without_parameters = type_name_without_parameters;

    const auto tmp_type_without_parameters_id = convertUnparametrizedTypeNameToTypeId(tmp_type_name_without_parameters);

    if (huge_int_as_string && tmp_type_without_parameters_id == DataSourceTypeId::UInt64) {
        tmp_type_name = "String";
        tmp_type_name_without_parameters = "String";
    }

    return getParent().getTypeInfo(tmp_type_name, tmp_type_name_without_parameters);
}

Poco::URI Connection::getUri() const {
    Poco::URI uri(url);

    if (!proto.empty())
        uri.setScheme(proto);

    if (!server.empty())
        uri.setHost(server);

    if (port != 0)
        uri.setPort(port);

    if (!path.empty())
        uri.setPath(path);

    bool database_set = false;
    bool default_format_set = false;

    for (const auto& parameter : uri.getQueryParameters()) {
        if (Poco::UTF8::icompare(parameter.first, "default_format") == 0) {
            default_format_set = true;
        }
        else if (Poco::UTF8::icompare(parameter.first, "database") == 0) {
            database_set = true;
        }
    }

    if (!default_format_set)
        uri.addQueryParameter("default_format", default_format);

    if (!database_set)
        uri.addQueryParameter("database", database);

    // To use some features of CH (e.g. TEMPORARY TABLEs) we need a (named) session.
    {
        const auto & parameters = uri.getQueryParameters();
        const auto p = std::find_if(parameters.begin(), parameters.end(), [](const auto & param_kv) {
            return param_kv.first == "session_id";
        });

        // DO not overwrite user-set session_id, just in case...
        if (p == parameters.end()) {
            uri.addQueryParameter("session_id", session_id);
        }
    }

    return uri;
}

void Connection::connect(const std::string & connection_string) {
    if (session && session->connected())
        throw SqlException("Connection name in use", "08002");

    auto cs_fields = readConnectionString(connection_string);

    const auto driver_cs_it = cs_fields.find(INI_DRIVER);
    const auto filedsn_cs_it = cs_fields.find(INI_FILEDSN);
    const auto savefile_cs_it = cs_fields.find(INI_SAVEFILE);
    const auto dsn_cs_it = cs_fields.find(INI_DSN);

    if (filedsn_cs_it != cs_fields.end())
        throw SqlException("Optional feature not implemented", "HYC00");

    if (savefile_cs_it != cs_fields.end())
        throw SqlException("Optional feature not implemented", "HYC00");

    key_value_map_t dsn_fields;

    // DRIVER and DSN won't exist in the field map at the same time, readConnectionString() will take care of that.
    if (driver_cs_it == cs_fields.end()) {
        std::string dsn_cs_val;

        if (dsn_cs_it != cs_fields.end())
            dsn_cs_val = dsn_cs_it->second;

        dsn_fields = readDSNInfo(dsn_cs_val);

        // Remove common but unused keys, if any.
        dsn_fields.erase(INI_DRIVER);
        dsn_fields.erase(INI_DESC);

        // Report and remove totally unexpected keys, if any.

        if (dsn_fields.find(INI_DSN) != dsn_fields.end()) {
            LOG("Unexpected key " << INI_DSN << " in DSN, ignoring");
            dsn_fields.erase(INI_DSN);
        }

        if (dsn_fields.find(INI_FILEDSN) != dsn_fields.end()) {
            LOG("Unexpected key " << INI_FILEDSN << " in DSN, ignoring");
            dsn_fields.erase(INI_FILEDSN);
        }

        if (dsn_fields.find(INI_SAVEFILE) != dsn_fields.end()) {
            LOG("Unexpected key " << INI_SAVEFILE << " in DSN, ignoring");
            dsn_fields.erase(INI_SAVEFILE);
        }
    }
    else {
        // Remove common but unused key.
        cs_fields.erase(driver_cs_it);
    }

    resetConfiguration();
    setConfiguration(cs_fields, dsn_fields);

    LOG("Creating session with " << proto << "://" << server << ":" << port);

#if !defined(WORKAROUND_DISABLE_SSL)
    const auto is_ssl = (Poco::UTF8::icompare(proto, "https") == 0);
    if (is_ssl) {
        const auto ssl_strict = (Poco::UTF8::icompare(sslmode, "allow") != 0);
        std::call_once(ssl_init_once, SSLInit, ssl_strict, privateKeyFile, certificateFile, caLocation);
    }
#endif

    session = (
#if !defined(WORKAROUND_DISABLE_SSL)
        is_ssl ? std::make_unique<Poco::Net::HTTPSClientSession>() :
#endif
        std::make_unique<Poco::Net::HTTPClientSession>()
    );

    session->setHost(server);
    session->setPort(port);
    session->setKeepAlive(true);
    session->setTimeout(Poco::Timespan(connection_timeout, 0), Poco::Timespan(timeout, 0), Poco::Timespan(timeout, 0));
    session->setKeepAliveTimeout(Poco::Timespan(86400, 0));

    if (verify_connection_early) {
        verifyConnection();
    }
}

void Connection::resetConfiguration() {
    dsn.clear();
    url.clear();
    proto.clear();
    username.clear();
    password.clear();
    server.clear();
    port = 0;
    connection_timeout = 0;
    timeout = 0;
    sslmode.clear();
    privateKeyFile.clear();
    certificateFile.clear();
    caLocation.clear();
    path.clear();
    default_format.clear();
    database.clear();
    stringmaxlength = 0;
}

void Connection::setConfiguration(const key_value_map_t & cs_fields, const key_value_map_t & dsn_fields) {
    // Returns tuple of bools: ("recognized key", "valid value").
    auto set_config_value = [&] (const std::string & key, const std::string & value) {
        bool recognized_key = false;
        bool valid_value = false;

        if (Poco::UTF8::icompare(key, INI_DSN) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value) {
                dsn = value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_URL) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value) {
                url = value;
            }
        }
        else if (
            Poco::UTF8::icompare(key, INI_UID) == 0 ||
            Poco::UTF8::icompare(key, INI_USERNAME) == 0
        ) {
            recognized_key = true;
            valid_value = (value.find(':') == std::string::npos);
            if (valid_value) {
                username = value;
            }
        }
        else if (
            Poco::UTF8::icompare(key, INI_PWD) == 0 ||
            Poco::UTF8::icompare(key, INI_PASSWORD) == 0
        ) {
            recognized_key = true;
            valid_value = true;
            if (valid_value) {
                password = value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_PROTO) == 0) {
            recognized_key = true;
            valid_value = (
                value.empty() ||
                Poco::UTF8::icompare(value, "http") == 0 ||
                Poco::UTF8::icompare(value, "https") == 0
            );
            if (valid_value) {
                proto = value;
            }
        }
        else if (
            Poco::UTF8::icompare(key, INI_SERVER) == 0 ||
            Poco::UTF8::icompare(key, INI_HOST) == 0
        ) {
            recognized_key = true;
            valid_value = true;
            if (valid_value) {
                server = value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_PORT) == 0) {
            recognized_key = true;
            unsigned int typed_value = 0;
            valid_value = (value.empty() || (
                Poco::NumberParser::tryParseUnsigned(value, typed_value) &&
                typed_value > 0 &&
                typed_value <= std::numeric_limits<decltype(port)>::max()
            ));
            if (valid_value) {
                port = typed_value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_TIMEOUT) == 0) {
            recognized_key = true;
            unsigned int typed_value = 0;
            valid_value = (value.empty() || (
                Poco::NumberParser::tryParseUnsigned(value, typed_value) &&
                typed_value <= std::numeric_limits<decltype(timeout)>::max()
            ));
            if (valid_value) {
                timeout = typed_value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_VERIFY_CONNECTION_EARLY) == 0) {
            recognized_key = true;
            valid_value = (value.empty() || isYesOrNo(value));
            if (valid_value) {
                verify_connection_early = isYes(value);
            }
        }
        else if (Poco::UTF8::icompare(key, INI_SSLMODE) == 0) {
            recognized_key = true;
            valid_value = (
                value.empty() ||
                Poco::UTF8::icompare(value, "allow") == 0 ||
                Poco::UTF8::icompare(value, "prefer") == 0 ||
                Poco::UTF8::icompare(value, "require") == 0
            );
            if (valid_value) {
                sslmode = value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_PRIVATEKEYFILE) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value) {
                privateKeyFile = value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_CERTIFICATEFILE) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value) {
                certificateFile = value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_CALOCATION) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value) {
                caLocation = value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_PATH) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value) {
                path = value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_DATABASE) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value) {
                database = value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_HUGE_INT_AS_STRING) == 0) {
            recognized_key = true;
            valid_value = (value.empty() || isYesOrNo(value));
            if (valid_value) {
                huge_int_as_string = isYes(value);
            }
        }
        else if (Poco::UTF8::icompare(key, INI_STRINGMAXLENGTH) == 0) {
            recognized_key = true;
            unsigned int typed_value = 0;
            valid_value = (value.empty() || (
                Poco::NumberParser::tryParseUnsigned(value, typed_value) &&
                typed_value > 0 &&
                typed_value <= std::numeric_limits<decltype(stringmaxlength)>::max()
            ));
            if (valid_value) {
                stringmaxlength = typed_value;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_DRIVERLOGFILE) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value) {
                getDriver().setAttr(CH_SQL_ATTR_DRIVERLOGFILE, value);
            }
        }
        else if (Poco::UTF8::icompare(key, INI_DRIVERLOG) == 0) {
            recognized_key = true;
            valid_value = (value.empty() || isYesOrNo(value));
            if (valid_value) {
                getDriver().setAttr(CH_SQL_ATTR_DRIVERLOG, (isYes(value) ? SQL_OPT_TRACE_ON : SQL_OPT_TRACE_OFF));
            }
        }

        return std::make_tuple(recognized_key, valid_value);
    };

    // Set recognised attributes from the DSN. Throw on invalid value. (This will overwrite the defaults.)
    for (auto & field : dsn_fields) {
        const auto & key = field.first;
        const auto & value = field.second;

        if (cs_fields.find(key) != cs_fields.end()) {
            LOG("DSN: attribute '" << key << " = " << value << "' unused, overriden by the connection string");
        }
        else {
            const auto res = set_config_value(key, value);
            const auto & recognized_key = std::get<0>(res);
            const auto & valid_value = std::get<1>(res);

            if (recognized_key) {
                if (!valid_value)
                    throw std::runtime_error("DSN: bad value '" + value + "' for attribute '" + key + "'");
            }
            else {
                LOG("DSN: unknown attribute '" << key << "', ignoring");
            }
        }
    }

    // Set recognised attributes from the connection string. Throw on invalid value. (This will overwrite the defaults, and those set from the DSN.)
    for (auto & field : cs_fields) {
        const auto & key = field.first;
        const auto & value = field.second;

        if (dsn_fields.find(key) != dsn_fields.end()) {
            LOG("Connection string: attribute '" << key << " = " << value << "' overrides DSN attribute with the same name");
        }

        const auto res = set_config_value(key, value);
        const auto & recognized_key = std::get<0>(res);
        const auto & valid_value = std::get<1>(res);

        if (recognized_key) {
            if (!valid_value)
                throw std::runtime_error("Connection string: bad value '" + value + "' for attribute '" + key + "'");
        }
        else {
            LOG("Connection string: unknown attribute '" << key << "', ignoring");
        }
    }

    // Deduce and set all the remaining attributes that are still carrying the default/unintialized values. (This will overwrite only some of the defaults.)

    if (dsn.empty())
        dsn = INI_DSN_DEFAULT;

    if (!url.empty()) {
        Poco::URI uri(url);

        if (proto.empty())
            proto = uri.getScheme();

        const auto & user_info = uri.getUserInfo();
        const auto index = user_info.find(':');
        if (index != std::string::npos) {
            if (password.empty())
                password = user_info.substr(index + 1);

            if (username.empty())
                username = user_info.substr(0, index);
        }

        if (server.empty())
            server = uri.getHost();

        if (port == 0) {
            // TODO(dakovalkov): This doesn't work when you explicitly set 80 for http and 443 for https due to Poco's getPort() behavior.
            const auto tmp_port = uri.getPort();
            if (
                (Poco::UTF8::icompare(proto, "https") == 0 && tmp_port != 443) ||
                (Poco::UTF8::icompare(proto, "http") == 0 && tmp_port != 80)
            )
                port = tmp_port;
        }

        if (path.empty())
            path = uri.getPath();

        for (const auto& parameter : uri.getQueryParameters()) {
            if (Poco::UTF8::icompare(parameter.first, "default_format") == 0) {
                default_format = parameter.second;
            }
            else if (Poco::UTF8::icompare(parameter.first, "database") == 0) {
                database = parameter.second;
            }
        }
    }

    if (proto.empty()) {
        if (!sslmode.empty() || port == 443 || port == 8443)
            proto = "https";
        else
            proto = "http";
    }

    if (username.empty())
        username = "default";

    if (server.empty())
        server = "localhost";

    if (port == 0)
        port = (Poco::UTF8::icompare(proto, "https") == 0 ? 8443 : 8123);

    if (timeout == 0)
        timeout = 30;

    if (connection_timeout == 0)
        connection_timeout = timeout;

    if (path.empty())
        path = "query";

    if (path[0] != '/')
        path = "/" + path;

    if (default_format.empty())
        default_format = "ODBCDriver2";

    if (database.empty())
        database = "default";

    if (stringmaxlength == 0)
        stringmaxlength = TypeInfo::string_max_size;
}

void Connection::verifyConnection() {
    LOG("Verifying connection and credentials...");
    auto & statement = allocateChild<Statement>();

    try {
        statement.executeQuery("SELECT 1");
    }
    catch (...) {
        statement.deallocateSelf();
        throw;
    }

    statement.deallocateSelf();
}

std::string Connection::buildCredentialsString() const {
    std::ostringstream user_password_base64;
    Poco::Base64Encoder base64_encoder(user_password_base64, Poco::BASE64_URL_ENCODING);
    base64_encoder << username << ":" << password;
    base64_encoder.close();
    return user_password_base64.str();
}

std::string Connection::buildUserAgentString() const {
    std::ostringstream user_agent;
    user_agent << "clickhouse-odbc/" << VERSION_STRING << " (" << SYSTEM_STRING << ")";
#if defined(UNICODE)
    user_agent << " UNICODE";
#endif
    if (!useragent.empty())
        user_agent << " " << useragent;
    return user_agent.str();
}

void Connection::initAsAD(Descriptor & desc, bool user) {
    desc.resetAttrs();
    desc.setAttr(SQL_DESC_ALLOC_TYPE, (user ? SQL_DESC_ALLOC_USER : SQL_DESC_ALLOC_AUTO));
    desc.setAttr(SQL_DESC_ARRAY_SIZE, 1);
    desc.setAttr(SQL_DESC_ARRAY_STATUS_PTR, 0);
    desc.setAttr(SQL_DESC_BIND_OFFSET_PTR, 0);
    desc.setAttr(SQL_DESC_BIND_TYPE, SQL_BIND_TYPE_DEFAULT);
}

void Connection::initAsID(Descriptor & desc) {
    desc.resetAttrs();
    desc.setAttr(SQL_DESC_ALLOC_TYPE, SQL_DESC_ALLOC_AUTO);
    desc.setAttr(SQL_DESC_ARRAY_STATUS_PTR, 0);
    desc.setAttr(SQL_DESC_ROWS_PROCESSED_PTR, 0);
}

void Connection::initAsDesc(Descriptor & desc, SQLINTEGER role, bool user) {
    switch (role) {
        case SQL_ATTR_APP_ROW_DESC: {
            initAsAD(desc, user);
            break;
        }
        case SQL_ATTR_APP_PARAM_DESC: {
            initAsAD(desc, user);
            break;
        }
        case SQL_ATTR_IMP_ROW_DESC: {
            initAsID(desc);
            break;
        }
        case SQL_ATTR_IMP_PARAM_DESC: {
            initAsID(desc);
            break;
        }
    }
}

void Connection::initAsADRec(DescriptorRecord & rec) {
    rec.resetAttrs();
    rec.setAttr(SQL_DESC_TYPE, SQL_C_DEFAULT); // Also sets SQL_DESC_CONCISE_TYPE (to SQL_C_DEFAULT) and SQL_DESC_DATETIME_INTERVAL_CODE (to 0).
    rec.setAttr(SQL_DESC_OCTET_LENGTH_PTR, 0);
    rec.setAttr(SQL_DESC_INDICATOR_PTR, 0);
    rec.setAttr(SQL_DESC_DATA_PTR, 0);
}

void Connection::initAsIDRec(DescriptorRecord & rec) {
    rec.resetAttrs();
}

void Connection::initAsDescRec(DescriptorRecord & rec, SQLINTEGER desc_role) {
    switch (desc_role) {
        case SQL_ATTR_APP_ROW_DESC: {
            initAsADRec(rec);
            break;
        }
        case SQL_ATTR_APP_PARAM_DESC: {
            initAsADRec(rec);
            break;
        }
        case SQL_ATTR_IMP_ROW_DESC: {
            initAsIDRec(rec);
            break;
        }
        case SQL_ATTR_IMP_PARAM_DESC: {
            initAsIDRec(rec);
            rec.setAttr(SQL_DESC_PARAMETER_TYPE, SQL_PARAM_INPUT);
            break;
        }
    }
}

template <>
Descriptor& Connection::allocateChild<Descriptor>() {
    auto child_sptr = std::make_shared<Descriptor>(*this);
    auto& child = *child_sptr;
    auto handle = child.getHandle();
    descriptors.emplace(handle, std::move(child_sptr));
    return child;
}

template <>
void Connection::deallocateChild<Descriptor>(SQLHANDLE handle) noexcept {
    descriptors.erase(handle);
}

template <>
Statement& Connection::allocateChild<Statement>() {
    auto child_sptr = std::make_shared<Statement>(*this);
    auto& child = *child_sptr;
    auto handle = child.getHandle();
    statements.emplace(handle, std::move(child_sptr));
    return child;
}

template <>
void Connection::deallocateChild<Statement>(SQLHANDLE handle) noexcept {
    statements.erase(handle);
}
