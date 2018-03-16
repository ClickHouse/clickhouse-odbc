#include "connection.h"
#include "config.h"
#include "string_ref.h"
#include "utils.h"

#include <Poco/NumberParser.h>

#include <Poco/Net/HTTPClientSession.h>

#if __has_include("config_cmake.h")
#include "config_cmake.h"
#endif

#if USE_SSL
#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/InvalidCertificateHandler.h>
#include <Poco/Net/PrivateKeyPassphraseHandler.h>
#include <Poco/Net/SSLManager.h>
#endif


Connection::Connection(Environment & env_) : environment(env_) {}

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

#if USE_SSL
    bool is_ssl = proto == "https";

    std::call_once(ssl_init_once, SSLInit);
#endif

    session = std::unique_ptr<Poco::Net::HTTPClientSession>(
#if USE_SSL
        is_ssl ? new Poco::Net::HTTPSClientSession :
#endif
               new Poco::Net::HTTPClientSession);

    session->setHost(server);
    session->setPort(port);
    session->setKeepAlive(true);
    session->setTimeout(Poco::Timespan(timeout, 0));
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
        if (current_key == "UID")
            user = current_value.toString();
        else if (current_key == "PWD")
            password = current_value.toString();
        else if (current_key == "PROTO")
            proto = current_value.toString();
        else if (current_key == "SSLMODE" && current_value == "require")
            proto = "https";
        else if (current_key == "HOST" || current_key == "SERVER")
            server = current_value.toString();
        else if (current_key == "PORT") {
            int int_port = 0;
            if (Poco::NumberParser::tryParse(current_value.toString(), int_port))
                port = int_port;
            else {
                throw std::runtime_error("Cannot parse port number.");
            }
        } else if (current_key == "DATABASE")
            database = current_value.toString();
        else if (current_key == "DSN")
            data_source = current_value.toString();
    }

    init();
}

void Connection::loadConfiguration() {
    if (data_source.empty())
        data_source = "ClickHouse";

    ConnInfo ci;
    stringToTCHAR(data_source, ci.dsn);
    getDSNinfo(&ci, true);

    if (!port && ci.port[0] != 0) {
        int int_port = 0;
        if (Poco::NumberParser::tryParse(stringFromTCHAR(ci.port), int_port))
            port = int_port;
        else
            throw std::runtime_error("Cannot parse port number.");
    }
    if (timeout == 0) {
        const std::string timeout_string = stringFromTCHAR(ci.timeout);
        if (!timeout_string.empty()) {
            if (!Poco::NumberParser::tryParse(timeout_string, this->timeout))
                throw std::runtime_error("Cannot parse connection timeout value.");
        }
    }

    if (server.empty())
        server = stringFromTCHAR(ci.server);
    if (user.empty())
        user = stringFromTCHAR(ci.username);
    if (password.empty())
        password = stringFromTCHAR(ci.password);
    if (database.empty())
        database = stringFromTCHAR(ci.database);
    if (proto.empty() && (ci.sslmode == std::string("require") || port == 8443))
        proto = "https";
}

void Connection::setDefaults() {
    if (data_source.empty())
        data_source = "ClickHouse";
    if (proto.empty())
        proto = (port == 8443 ? "https" : "http");
    if (server.empty())
        server = "localhost";
    if (port == 0)
        port = (proto == "https" ? 8443 : 8123);
    if (user.empty())
        user = "default";
    if (database.empty())
        database = "default";
    if (timeout == 0)
        timeout = 30;
}

std::once_flag ssl_init_once;

void SSLInit() {
// http://stackoverflow.com/questions/18315472/https-request-in-c-using-poco
#if USE_SSL
    Poco::Net::initializeSSL();
    // TODO: not accept invalid cert by some settings
    Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> ptrHandler = new Poco::Net::AcceptCertificateHandler(false);
    Poco::Net::Context::Ptr ptrContext = new Poco::Net::Context(
        Poco::Net::Context::CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_RELAXED, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    Poco::Net::SSLManager::instance().initializeClient(0, ptrHandler, ptrContext);
#endif
}
