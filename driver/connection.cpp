#include "connection.h"
#include "utils.h"
#include "string_ref.h"
#include "config.h"
#include "descriptor.h"
#include "statement.h"

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

void SSLInit(bool ssl_strict, const std::string & privateKeyFile, const std::string & certificateFile, const std::string & caLocation) {
// http://stackoverflow.com/questions/18315472/https-request-in-c-using-poco
#if USE_SSL
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
#endif
}


Connection::Connection(Environment & environment)
    : ChildType(environment)
{
}

std::string Connection::connectionString() const {
    std::string ret;
    ret += "DSN=" + data_source + ";";
    ret += "DATABASE=" + database + ";";
    ret += "PROTO=" + proto + ";";
    ret += "SERVER=" + server + ";";
    ret += "PORT=" + std::to_string(port) + ";";
    ret += "UID=" + user + ";";
    if (!password.empty()) {
        ret += "PWD=" + password + ";";
    }
    return ret;
}

const std::string & Connection::getDatabase() const {
    return database;
}

void Connection::setDatabase(const std::string & db) {
    database = db;
}

void Connection::init() {
    loadConfiguration();
    setDefaults();

    if (user.find(':') != std::string::npos)
        throw std::runtime_error("Username couldn't contain ':' (colon) symbol.");

    LOG("Creating session with " << proto << "://" << server << ":" << port);

#if USE_SSL
    bool is_ssl = proto == "https";

    if (is_ssl)
        std::call_once(ssl_init_once, SSLInit, ssl_strict, privateKeyFile, certificateFile, caLocation);
#endif

    session = std::unique_ptr<Poco::Net::HTTPClientSession>(
#if USE_SSL
        is_ssl ? new Poco::Net::HTTPSClientSession :
#endif
               new Poco::Net::HTTPClientSession);

    session->setHost(server);
    session->setPort(port);
    session->setKeepAlive(true);
    session->setTimeout(Poco::Timespan(connection_timeout, 0), Poco::Timespan(timeout, 0), Poco::Timespan(timeout, 0));
    session->setKeepAliveTimeout(Poco::Timespan(86400, 0));
}

void Connection::init(const std::string & dsn_,
    const uint16_t port_,
    const std::string & user_,
    const std::string & password_,
    const std::string & database_) {
    if (session && session->connected())
        throw std::runtime_error("Already connected.");

    data_source = dsn_;

    if (port_)
        port = port_;
    if (!user_.empty())
        user = user_;
    if (!password_.empty())
        password = password_;
    if (!database_.empty())
        database = database_;

    init();
}

void Connection::init(const std::string & connection_string) {
    /// connection_string - string of the form `DSN=ClickHouse;UID=default;PWD=password`

    const char * pos = connection_string.data();
    const char * end = pos + connection_string.size();

    StringRef current_key;
    StringRef current_value;

    while ((pos = nextKeyValuePair(pos, end, current_key, current_value))) {
        auto key_lower = current_key.toString();
        std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);
        LOG("Parse DSN: key=" << key_lower << " value=" << current_value.toString());
        if (key_lower == "uid")
            user = current_value.toString();
        else if (key_lower == "pwd")
            password = current_value.toString();
        else if (key_lower == "proto")
            proto = current_value.toString();
        else if (key_lower == "sslmode" && (current_value == "allow" || current_value == "prefer" || current_value == "require")) {
            proto = "https";
            if (current_value == "require")
                ssl_strict = true;
        } else if (key_lower == "url")
            url = current_value.toString();
        else if (key_lower == "host" || key_lower == "server")
            server = current_value.toString();
        else if (key_lower == "port") {
            int int_val = 0;
            if (Poco::NumberParser::tryParse(current_value.toString(), int_val))
                port = int_val;
            else {
                throw std::runtime_error("Cannot parse port number.");
            }
        } else if (key_lower == "database")
            database = current_value.toString();
        else if (key_lower == "timeout") {
            int int_val = 0;
            if (Poco::NumberParser::tryParse(current_value.toString(), int_val))
                connection_timeout = timeout = int_val;
            else {
                throw std::runtime_error("Cannot parse timeout.");
            }
        } else if (key_lower == "stringmaxlength") {
            int int_val = 0;
            if (Poco::NumberParser::tryParse(current_value.toString(), int_val))
                stringmaxlength = int_val;
            else {
                throw std::runtime_error("Cannot parse stringmaxlength.");
            }
        } else if (key_lower == "dsn")
            data_source = current_value.toString();
        else if (key_lower == "privatekeyfile")
            privateKeyFile = current_value.toString();
        else if (key_lower == "certificatefile")
            certificateFile = current_value.toString();
        else if (key_lower == "calocation")
            caLocation = current_value.toString();
    }

    init();
}

void Connection::loadConfiguration() {
    if (data_source.empty())
        data_source = INI_DSN_DEFAULT;

    ConnInfo ci;
    stringToTCHAR(data_source, ci.dsn);
    getDSNinfo(&ci, true);

    {
        const std::string tracefile = stringFromMYTCHAR(ci.tracefile);
        if (!tracefile.empty()) {
            getDriver().setAttr(SQL_ATTR_TRACEFILE, tracefile);
        }

        const std::string trace = stringFromMYTCHAR(ci.trace);
        if (!trace.empty()) {
            getDriver().setAttr(SQL_ATTR_TRACE, (isYes(trace) ? SQL_OPT_TRACE_ON : SQL_OPT_TRACE_OFF));
        }
    }

    if (url.empty())
        url = stringFromMYTCHAR(ci.url);
    if (!port && ci.port[0] != 0) {
        const std::string string = stringFromMYTCHAR(ci.port);
        if (!string.empty()) {
            int tmp = 0;
            if (!Poco::NumberParser::tryParse(string, tmp))
                throw std::runtime_error(("Cannot parse port number [" + string + "].").c_str());
            port = tmp;
        }
    }
    if (timeout == 0) {
        const std::string string = stringFromMYTCHAR(ci.timeout);
        if (!string.empty()) {
            if (!Poco::NumberParser::tryParse(string, this->timeout))
                throw std::runtime_error("Cannot parse connection timeout value [" + string + "].");
            this->connection_timeout = this->timeout;
        }
    }
    if (stringmaxlength == 0) {
        const std::string string = stringFromMYTCHAR(ci.stringmaxlength);
        if (!string.empty()) {
            if (!Poco::NumberParser::tryParse(string, this->stringmaxlength))
                throw std::runtime_error("Cannot parse stringmaxlength value [" + string + "].");
        }
    }

    if (server.empty())
        server = stringFromMYTCHAR(ci.server);
    if (user.empty())
        user = stringFromMYTCHAR(ci.username);
    if (password.empty())
        password = stringFromMYTCHAR(ci.password);
    if (database.empty())
        database = stringFromMYTCHAR(ci.database);
    auto sslmode = stringFromMYTCHAR(ci.sslmode);
    if (proto.empty() && (sslmode == "require" || sslmode == "prefer" || sslmode == "allow" || port == 8443))
        proto = "https";
    if (sslmode == "require")
        ssl_strict = true;

    if (privateKeyFile.empty())
        privateKeyFile = stringFromMYTCHAR(ci.privateKeyFile);
    if (certificateFile.empty())
        certificateFile = stringFromMYTCHAR(ci.certificateFile);
    if (caLocation.empty())
        caLocation = stringFromMYTCHAR(ci.caLocation);
}

void Connection::setDefaults() {
    if (data_source.empty())
        data_source = INI_DSN_DEFAULT;
    if (!url.empty()) {
        Poco::URI uri(url);
        if (proto.empty())
            proto = uri.getScheme();
        if (server.empty())
            server = uri.getHost();
        if (port == 0) {
            // TODO(dakovalkov): This doesn't work when you explicitly set 80 for http and 433 for https.
            const auto tmp_port = uri.getPort();
            if ((proto == "https" && tmp_port != 443) || (proto == "http" && tmp_port != 80))
                port = tmp_port;
        }
        if (path.empty())
            path = uri.getPath();

        for (const auto& parameter : uri.getQueryParameters()) {
            if (parameter.first == "database") {
                database = parameter.second;
            }
        }

        auto user_info = uri.getUserInfo();
        auto index = user_info.find(':');
        if (index != std::string::npos) {
            if (password.empty())
                password = user_info.substr(index + 1);
            if (user.empty())
                user = user_info.substr(0, index);
        }
    }

    if (proto.empty())
        proto = (port == 8443 ? "https" : "http");
    if (server.empty())
        server = "localhost";
    if (port == 0)
        port = (proto == "https" ? 8443 : 8123);
    if (path.empty())
        path = "query";
    if (path[0] != '/')
        path = "/" + path;
    if (stringmaxlength == 0)
        stringmaxlength = Environment::string_max_size;
    if (user.empty())
        user = "default";
    if (database.empty())
        database = "default";
    if (timeout == 0)
        timeout = 30;
    if (connection_timeout == 0)
        connection_timeout = timeout;
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
