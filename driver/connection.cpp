#include "driver/utils/utils.h"
#include "driver/config/ini_defines.h"
#include "driver/connection.h"
#include "driver/descriptor.h"
#include "driver/statement.h"

#include <Poco/Base64Encoder.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/NumberParser.h> // TODO: switch to std
#include <Poco/URI.h>

#if USE_SSL
#    include <Poco/Net/AcceptCertificateHandler.h>
#    include <Poco/Net/RejectCertificateHandler.h>

#    include <Poco/Net/HTTPSClientSession.h>
#    include <Poco/Net/InvalidCertificateHandler.h>
#    include <Poco/Net/PrivateKeyPassphraseHandler.h>
#    include <Poco/Net/SSLManager.h>
#endif

std::once_flag ssl_init_once;

#if USE_SSL
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

Connection::Connection(Environment & environment)
    : ChildType(environment)
{
    resetConfiguration();
}

void Connection::connect(const std::string & connection_string) {
    if (session && session->connected())
        throw SqlException("Connection name in use", "08002");

    auto cs_fields = readConnectionString(connection_string);

    const auto driver_cs_it = cs_fields.find(INI_DRIVER);
    const auto dsn_cs_it = cs_fields.find(INI_DSN);
    const auto filedsn_cs_it = cs_fields.find(INI_FILEDSN);
    const auto savefile_cs_it = cs_fields.find(INI_SAVEFILE);

    if (filedsn_cs_it != cs_fields.end())
        throw SqlException("Optional feature not implemented", "HYC00");

    if (savefile_cs_it != cs_fields.end())
        throw SqlException("Optional feature not implemented", "HYC00");

    key_value_map_t dsn_fields;

    // DRIVER and DSN won't exist in the field map at the same time, readConnectionString() will take care of that.
    if (driver_cs_it == cs_fields.end()) {
        std::string dsn;

        if (dsn_cs_it != cs_fields.end())
            dsn = dsn_cs_it->second;

        dsn_fields = readDSNInfo(dsn);

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

#if USE_SSL
    const auto is_ssl = (Poco::UTF8::icompare(proto, "https") == 0);
    if (is_ssl) {
        const auto ssl_strict = (Poco::UTF8::icompare(sslmode, "allow") != 0);
        std::call_once(ssl_init_once, SSLInit, ssl_strict, privateKeyFile, certificateFile, caLocation);
    }
#endif

    session = std::unique_ptr<Poco::Net::HTTPClientSession>(
#if USE_SSL
        is_ssl ? new Poco::Net::HTTPSClientSession :
#endif
        new Poco::Net::HTTPClientSession
    );

    session->setHost(server);
    session->setPort(port);
    session->setKeepAlive(true);
    session->setTimeout(Poco::Timespan(connection_timeout, 0), Poco::Timespan(timeout, 0), Poco::Timespan(timeout, 0));
    session->setKeepAliveTimeout(Poco::Timespan(86400, 0));
}

void Connection::resetConfiguration() {
    data_source.clear();
    url.clear();
    proto.clear();
    server.clear();
    port = 0;
    path.clear();
    user.clear();
    password.clear();
    database.clear();
    timeout = 0;
    connection_timeout = 0;
    stringmaxlength = 0;
    sslmode.clear();
    privateKeyFile.clear();
    certificateFile.clear();
    caLocation.clear();
}

void Connection::setConfiguration(const key_value_map_t & cs_fields, const key_value_map_t & dsn_fields) {
    // Returns tuple of bools: ("recognized key", "valid value", "value overwritten").
    auto set_config_value = [&] (const std::string & key, const std::string & value, bool overwrite) {
        bool recognized_key = false;
        bool valid_value = false;
        bool value_overwritten = false;

        if (Poco::UTF8::icompare(key, INI_DSN) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value && (overwrite || data_source.empty())) {
                data_source = value;
                value_overwritten = true;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_URL) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value && (overwrite || url.empty())) {
                url = value;
                value_overwritten = true;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_PROTO) == 0) {
            recognized_key = true;
            valid_value = (
                value.empty() ||
                Poco::UTF8::icompare(value, "http") == 0 ||
                Poco::UTF8::icompare(value, "https") == 0
            );
            if (valid_value && (overwrite || proto.empty())) {
                proto = value;
                value_overwritten = true;
            }
        }
        else if (
            Poco::UTF8::icompare(key, INI_SERVER) == 0 ||
            Poco::UTF8::icompare(key, INI_HOST) == 0
        ) {
            recognized_key = true;
            valid_value = true;
            if (valid_value && (overwrite || server.empty())) {
                server = value;
                value_overwritten = true;
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
            if (valid_value && (overwrite || port == 0)) {
                port = typed_value;
                value_overwritten = true;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_PATH) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value && (overwrite || path.empty())) {
                path = value;
                value_overwritten = true;
            }
        }
        else if (
            Poco::UTF8::icompare(key, INI_UID) == 0 ||
            Poco::UTF8::icompare(key, INI_USERNAME) == 0
        ) {
            recognized_key = true;
            valid_value = (value.find(':') == std::string::npos);
            if (valid_value && (overwrite || user.empty())) {
                user = value;
                value_overwritten = true;
            }
        }
        else if (
            Poco::UTF8::icompare(key, INI_PWD) == 0 ||
            Poco::UTF8::icompare(key, INI_PASSWORD) == 0
        ) {
            recognized_key = true;
            valid_value = true;
            if (valid_value && (overwrite || password.empty())) {
                password = value;
                value_overwritten = true;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_DATABASE) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value && (overwrite || database.empty())) {
                database = value;
                value_overwritten = true;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_TIMEOUT) == 0) {
            recognized_key = true;
            unsigned int typed_value = 0;
            valid_value = (value.empty() || (
                Poco::NumberParser::tryParseUnsigned(value, typed_value) &&
                typed_value <= std::numeric_limits<decltype(timeout)>::max()
            ));
            if (valid_value && (overwrite || timeout == 0)) {
                timeout = typed_value;
                value_overwritten = true;
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
            if (valid_value && (overwrite || stringmaxlength == 0)) {
                stringmaxlength = typed_value;
                value_overwritten = true;
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
            if (valid_value && (overwrite || sslmode.empty())) {
                sslmode = value;
                value_overwritten = true;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_PRIVATEKEYFILE) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value && (overwrite || privateKeyFile.empty())) {
                privateKeyFile = value;
                value_overwritten = true;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_CERTIFICATEFILE) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value && (overwrite || certificateFile.empty())) {
                certificateFile = value;
                value_overwritten = true;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_CALOCATION) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value && (overwrite || caLocation.empty())) {
                caLocation = value;
                value_overwritten = true;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_DRIVERLOGFILE) == 0) {
            recognized_key = true;
            valid_value = true;
            if (valid_value && (overwrite || !getDriver().hasAttr(CH_SQL_ATTR_DRIVERLOGFILE))) {
                getDriver().setAttr(CH_SQL_ATTR_DRIVERLOGFILE, value);
                value_overwritten = true;
            }
        }
        else if (Poco::UTF8::icompare(key, INI_DRIVERLOG) == 0) {
            recognized_key = true;
            valid_value = (value.empty() || isYesOrNo(value));
            if (valid_value && (overwrite || !getDriver().hasAttr(CH_SQL_ATTR_DRIVERLOG))) {
                getDriver().setAttr(CH_SQL_ATTR_DRIVERLOG, (isYes(value) ? SQL_OPT_TRACE_ON : SQL_OPT_TRACE_OFF));
                value_overwritten = true;
            }
        }

        return std::make_tuple(recognized_key, valid_value, value_overwritten);
    };

    for (auto & field : cs_fields) {
        const auto & key = field.first;
        const auto & value = field.second;
        const auto res = set_config_value(key, value, true);
        const auto & recognized_key = std::get<0>(res);
        const auto & valid_value = std::get<1>(res);
        const auto & value_overwritten = std::get<2>(res);

        if (recognized_key) {
            if (valid_value) {
                LOG("Connection string attribute" << (value_overwritten ? "" : " (unused)") << ": " << key << " = " << value);
            }
            else {
                throw std::runtime_error("bad value '" + value + "' for connection string attribute '" + key + "'");
            }
        }
        else {
            LOG("Connection string: unknown attribute '" << key << "'");
        }
    }

    for (auto & field : dsn_fields) {
        const auto & key = field.first;
        const auto & value = field.second;
        const auto res = set_config_value(key, value, false);
        const auto & recognized_key = std::get<0>(res);
        const auto & valid_value = std::get<1>(res);
        const auto & value_overwritten = std::get<2>(res);

        if (recognized_key) {
            if (valid_value) {
                LOG("DSN attribute" << (value_overwritten ? "" : " (unused, overriden by the connection string)") << ": " << key << " = " << value);
            }
            else {
                throw std::runtime_error("bad value '" + value + "' for DSN attribute '" + key + "'");
            }
        }
        else {
            LOG("DSN: unknown attribute '" << key << "'");
        }
    }

    if (data_source.empty())
        data_source = INI_DSN_DEFAULT;

    if (!url.empty()) {
        Poco::URI uri(url);

        if (proto.empty())
            proto = uri.getScheme();

        const auto & user_info = uri.getUserInfo();
        const auto index = user_info.find(':');
        if (index != std::string::npos) {
            if (password.empty())
                password = user_info.substr(index + 1);

            if (user.empty())
                user = user_info.substr(0, index);
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
            if (parameter.first == "database") {
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

    if (user.empty())
        user = "default";

    if (server.empty())
        server = "localhost";

    if (port == 0)
        port = (Poco::UTF8::icompare(proto, "https") == 0 ? 8443 : 8123);

    if (path.empty())
        path = "query";

    if (path[0] != '/')
        path = "/" + path;

    if (database.empty())
        database = "default";

    if (timeout == 0)
        timeout = 30;

    if (connection_timeout == 0)
        connection_timeout = timeout;

    if (stringmaxlength == 0)
        stringmaxlength = TypeInfo::string_max_size;
}

std::string Connection::buildCredentialsString() const {
    std::ostringstream user_password_base64;
    Poco::Base64Encoder base64_encoder(user_password_base64, Poco::BASE64_URL_ENCODING);
    base64_encoder << user << ":" << password;
    base64_encoder.close();
    return user_password_base64.str();
}

std::string Connection::buildUserAgentString() const {
    std::ostringstream user_agent;
    user_agent << "clickhouse-odbc/" << VERSION_STRING << " (" << CMAKE_SYSTEM << ")";
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
