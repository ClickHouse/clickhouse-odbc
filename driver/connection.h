#pragma once

#include <Poco/Net/HTTPClientSession.h>

#include "diagnostics.h"
#include "environment.h"

struct Connection
{
    Environment & environment;

    std::string host = "localhost";
    uint16_t port = 8123;
    std::string user = "default";
    std::string password;
    std::string database = "default";
    std::string data_source = "ClickHouse";

    Poco::Net::HTTPClientSession session;
    DiagnosticRecord diagnostic_record;
    int retry_count = 3;

    Connection(Environment & env_);

    void init();

    void init(
        const std::string & host_,
        const uint16_t port_,
        const std::string & user_,
        const std::string & password_,
        const std::string & database_);

    void init(const std::string & connection_string);
};
