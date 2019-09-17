#pragma once

#include "driver.h"
#include "environment.h"

#include <memory>
#include <mutex>

#include <Poco/Net/HTTPClientSession.h>

class DescriptorRecord;
class Descriptor;
class Statement;

class Connection
    : public Child<Environment, Connection>
{
private:
    using ChildType = Child<Environment, Connection>;

public:
    std::string data_source;
    std::string url;
    std::string proto;
    std::string server;
    std::string path;
    std::string user;
    std::string password;
    uint16_t port = 0;
    int timeout = 0;
    int connection_timeout = 0;
    int32_t stringmaxlength = 0;
    bool ssl_strict = false;

    std::string privateKeyFile;
    std::string certificateFile;
    std::string caLocation;

    std::string useragent;

    std::unique_ptr<Poco::Net::HTTPClientSession> session;
    int retry_count = 3;

public:
    explicit Connection(Environment & environment);

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

    // Return a Base64 encoded string of "user:password".
    std::string buildCredentialsString() const;

    // Return a crafted User-Agent string.
    std::string buildUserAgentString() const;

    // Reset the descriptor and initialize it with default attributes.
    void init_as_ad(Descriptor & desc, bool user = false); // as Application Descriptor
    void init_as_id(Descriptor & desc); // as Implementation Descriptor

    // Reset the descriptor and initialize it with default attributes.
    void init_as_desc(Descriptor & desc, SQLINTEGER role, bool user = false); // ARD, APD, IRD, IPD

    // Reset the descriptor record and initialize it with default attributes.
    void init_as_ad_rec(DescriptorRecord & rec); // as a record of Application Descriptor
    void init_as_id_rec(DescriptorRecord & rec); // as a record of Implementation Descriptor

    // Reset the descriptor record and initialize it with default attributes.
    void init_as_desc_rec(DescriptorRecord & rec, SQLINTEGER desc_role); // ARD, APD, IRD, IPD

    // Leave unimplemented for general case.
    template <typename T> T& allocate_child();
    template <typename T> void deallocate_child(SQLHANDLE) noexcept;

private:
    /// Load uninitialized fields from odbc.ini
    void loadConfiguration();

    /// Sets uninitialized fields to their default values.
    void setDefaults();

private:
    std::string database;
    std::unordered_map<SQLHANDLE, std::shared_ptr<Descriptor>> descriptors;
    std::unordered_map<SQLHANDLE, std::shared_ptr<Statement>> statements;
};

template <> Descriptor& Connection::allocate_child<Descriptor>();
template <> void Connection::deallocate_child<Descriptor>(SQLHANDLE handle) noexcept;

template <> Statement& Connection::allocate_child<Statement>();
template <> void Connection::deallocate_child<Statement>(SQLHANDLE handle) noexcept;
