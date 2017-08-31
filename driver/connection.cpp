#include "config.h"
#include "connection.h"
#include "string_ref.h"
#include "utils.h"

#include <Poco/NumberParser.h>

Connection::Connection(Environment & env_)
    : environment(env_)
{
}

std::string Connection::connectionString() const
{
    std::string ret;
    ret += "DSN=" + data_source + ";";
    ret += "DATABASE=" + database + ";";
    ret += "SERVER=" + server + ";";
    ret += "PORT=" + std::to_string(port) + ";";
    ret += "UID=" + user + ";";
    // TODO (artpaul) Do we need to fill PWD?
    return ret;
}

const std::string & Connection::getDatabase() const
{
    return database;
}

void Connection::setDatabase(const std::string & db)
{
    database = db;
}

void Connection::init()
{
    loadConfiguration();
    setDefaults();

    if (user.find(':') != std::string::npos)
        throw std::runtime_error("Username couldn't contain ':' (colon) symbol.");

    session.setHost(server);
    session.setPort(port);
    session.setKeepAlive(true);
    session.setTimeout(Poco::Timespan(30, 0));
    session.setKeepAliveTimeout(Poco::Timespan(86400, 0));
}

void Connection::init(
    const std::string & dsn_,
    const uint16_t port_,
    const std::string & user_,
    const std::string & password_,
    const std::string & database_)
{
    if (session.connected())
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

void Connection::init(const std::string & connection_string)
{
    /// connection_string - string of the form `DSN=ClickHouse;UID=default;PWD=password`

    const char * pos = connection_string.data();
    const char * end = pos + connection_string.size();

    StringRef current_key;
    StringRef current_value;

    while ((pos = nextKeyValuePair(pos, end, current_key, current_value)))
    {
        if (current_key == "UID")
            user = current_value.toString();
        else if (current_key == "PWD")
            password = current_value.toString();
        else if (current_key == "HOST" || current_key == "SERVER")
            server = current_value.toString();
        else if (current_key == "PORT")
        {
            int int_port = 0;
            if (Poco::NumberParser::tryParse(current_value.toString(), int_port))
                port = int_port;
            else
                throw std::runtime_error("Cannot parse port number.");
        }
        else if (current_key == "DATABASE")
            database = current_value.toString();
        else if (current_key == "DSN")
            data_source = current_value.toString();
    }

    init();
}

void Connection::loadConfiguration()
{
    if (data_source.empty())
        data_source = "ClickHouse";

    ConnInfo ci;
    stringToTCHAR(data_source, ci.dsn);
    getDSNinfo(&ci, true);

    if (!port)
    {
        int int_port = 0;
        if (Poco::NumberParser::tryParse(stringFromTCHAR(ci.port), int_port))
            port = int_port;
        else
            throw std::runtime_error("Cannot parse port number.");
    }

    if (server.empty())
        server = stringFromTCHAR(ci.server);
    if (user.empty())
        user = stringFromTCHAR(ci.username);
    if (password.empty())
        password = stringFromTCHAR(ci.password);
    if (database.empty())
        database = stringFromTCHAR(ci.database);
}

void Connection::setDefaults()
{
    if (data_source.empty())
        data_source = "ClickHouse";
    if (server.empty())
        server = "localhost";
    if (port == 0)
        port = 8123;
    if (user.empty())
        user = "default";
    if (database.empty())
        database = "default";
}
