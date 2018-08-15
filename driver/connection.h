#pragma once

#include <memory>
#include <mutex>
#include <Poco/Net/HTTPClientSession.h>

#include "diagnostics.h"
#include "environment.h"
//namespace Poco { namespace Net { class HTTPClientSession; } }

struct Connection {
    Environment & environment;

    std::string data_source;
    std::string proto;
    std::string server;
    std::string user;
    std::string password;
    uint16_t port = 0;
    int timeout = 0;
    int connection_timeout = 0;

    std::unique_ptr<Poco::Net::HTTPClientSession> session;
    DiagnosticRecord diagnostic_record;
    int retry_count = 3;

    Connection(Environment & env_);

    /// Returns the completed connection string.
    std::string connectionString() const;

    /// Returns database associated with the current connection.
    const std::string & getDatabase() const;

    /// Sets database to the current connection;
    void setDatabase(const std::string & db);

    void init();

    void init(const std::string & dsn_,
        const uint16_t port_,
        const std::string & user_,
        const std::string & password_,
        const std::string & database_);

    void init(const std::string & connection_string);

private:
    /// Load uninitialized fields from odbc.ini
    void loadConfiguration();

    /// Sets uninitialized fields to their default values.
    void setDefaults();

private:
    std::string database;
};

extern std::once_flag ssl_init_once;
void SSLInit();
