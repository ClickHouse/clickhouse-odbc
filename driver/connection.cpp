#include "config.h"
#include "connection.h"
#include "string_ref.h"
#include "utils.h"

#include <Poco/NumberParser.h>

Connection::Connection(Environment & env_)
    : environment(env_)
{
}

void Connection::init()
{
    if (user.find(':') != std::string::npos)
        throw std::runtime_error("Username couldn't contain ':' (colon) symbol.");

    session.setHost(host);
    session.setPort(port);
    session.setKeepAlive(true);
    session.setKeepAliveTimeout(Poco::Timespan(86400, 0));

    /// TODO Timeout.
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

    if (!dsn_.empty())
        data_source = dsn_;

    ConnInfo ci;
    stringToTCHAR(data_source, ci.dsn);
    getDSNinfo(&ci, true);

    if (port_)
        port = port_;
    else
    {
        int int_port = 0;
        if (Poco::NumberParser::tryParse(stringFromTCHAR(ci.port), int_port))
            port = int_port;
        else
            throw std::runtime_error("Cannot parse port number.");
    }

    if (!user_.empty())
        user = user_;
    else
        user = stringFromTCHAR(ci.username);

    if (!password_.empty())
        password = password_;
    else
        password = stringFromTCHAR(ci.password);

    if (!database_.empty())
        database = database_;
    else
        database = stringFromTCHAR(ci.database);

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
        else if (current_key == "HOST")
            host = current_value.toString();
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
