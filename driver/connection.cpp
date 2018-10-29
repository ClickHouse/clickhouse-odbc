#include "connection.h"
#include "config.h"
#include "string_ref.h"
#include "utils.h"

#include <Poco/NumberParser.h> // TODO: switch to std

#include <Poco/Net/HTTPClientSession.h>

//#if __has_include("config_cmake.h") // requre c++17
#if CMAKE_BUILD
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

    LOG("Creating session with " << proto << "://" << server << ":" << port);

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
    session->setTimeout(Poco::Timespan(connection_timeout, 0), Poco::Timespan(timeout, 0),Poco::Timespan(timeout, 0) );
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
        else if (key_lower == "sslmode" && current_value == "require")
            proto = "https";
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
        }
        else if (key_lower == "stringmaxlength") {
            int int_val = 0;
            if (Poco::NumberParser::tryParse(current_value.toString(), int_val))
                stringmaxlength = int_val;
            else {
                throw std::runtime_error("Cannot parse stringmaxlength.");
            }
        }
        else if (key_lower == "dsn")
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

    auto tracefile = stringFromMYTCHAR(ci.tracefile);
    {
        auto str = stringFromMYTCHAR(ci.trace);
        if (!str.empty())
            log_enabled = !(str == "0" || str == "No" || str == "no");
        if (log_enabled && !tracefile.empty() && tracefile != LOG_DEFAULT_FILE)
            logstream = std::ofstream(tracefile, std::ios::out | std::ios::app);
    }

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
    if (proto.empty() && (stringFromMYTCHAR(ci.sslmode) == "require" || port == 8443))
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

std::once_flag ssl_init_once;

void SSLInit() {
// http://stackoverflow.com/questions/18315472/https-request-in-c-using-poco
#if USE_SSL
    Poco::Net::initializeSSL();
    // TODO: not accept invalid cert by some settings
    Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> ptrHandler = new Poco::Net::AcceptCertificateHandler(false);
    Poco::Net::Context::Ptr ptrContext = new Poco::Net::Context(
		Poco::Net::Context::CLIENT_USE, ""
#if !defined(SECURITY_WIN32)
                // Do not work with poco/NetSSL_Win:
                , "", "", Poco::Net::Context::VERIFY_RELAXED, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"
#endif
	);
    Poco::Net::SSLManager::instance().initializeClient(0, ptrHandler, ptrContext);
#endif
}
