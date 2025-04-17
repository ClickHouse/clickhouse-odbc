#pragma once

#include "driver/driver.h"
#include "driver/environment.h"
#include "driver/config/config.h"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/URI.h>

#include <memory>
#include <mutex>

class DescriptorRecord;
class Descriptor;
class Statement;

class Connection
    : public Child<Environment, Connection>
{
private:
    using ChildType = Child<Environment, Connection>;
    const std::string session_id;
    std::string url;

public: // Configuration fields.
    std::string dsn;
    std::string proto;
    std::string username;
    std::string password;
    std::string server;
    std::uint16_t port = 0;
    std::uint32_t connection_timeout = 0;
    std::uint32_t timeout = 0;
    bool verify_connection_early = false;
    std::string sslmode;
    std::string privateKeyFile;
    std::string certificateFile;
    std::string caLocation;
    std::string path;
    std::string default_format;
    std::string database;
    bool huge_int_as_string = false;
    std::int32_t stringmaxlength = 0;
    bool auto_session_id = false;

public:
    std::string useragent;

    std::unique_ptr<Poco::Net::HTTPClientSession> session;
    int retry_count = 3;
    int redirect_limit = 10;

public:
    explicit Connection(Environment & environment);

    // Lookup TypeInfo for given name of type.
    const TypeInfo & getTypeInfo(const std::string & type_name, const std::string & type_name_without_parameters) const;

    Poco::URI getUri() const;

    void connect(const std::string & connection_string);

    // Return a Base64 encoded string of "user:password".
    std::string buildCredentialsString() const;

    // Return a crafted User-Agent string.
    std::string buildUserAgentString() const;

    // Reset the descriptor and initialize it with default attributes.
    void initAsAD(Descriptor & desc, bool user = false); // as Application Descriptor
    void initAsID(Descriptor & desc); // as Implementation Descriptor

    // Reset the descriptor and initialize it with default attributes.
    void initAsDesc(Descriptor & desc, SQLINTEGER role, bool user = false); // ARD, APD, IRD, IPD

    // Reset the descriptor record and initialize it with default attributes.
    void initAsADRec(DescriptorRecord & rec); // as a record of Application Descriptor
    void initAsIDRec(DescriptorRecord & rec); // as a record of Implementation Descriptor

    // Reset the descriptor record and initialize it with default attributes.
    void initAsDescRec(DescriptorRecord & rec, SQLINTEGER desc_role); // ARD, APD, IRD, IPD

    // Leave unimplemented for general case.
    template <typename T> T & allocateChild();
    template <typename T> void deallocateChild(SQLHANDLE) noexcept;

private:
    // Reset all configuration fields to their default/unintialized values.
    void resetConfiguration();

    // Set configuration fields to:
    //     a) values from connection string, or
    //     b) values from DSN, if unintialized, or
    //     c) values deduced from values of other fields, if unintialized.
    void setConfiguration(const key_value_map_t & cs_fields, const key_value_map_t & dsn_fields);

    // Verify the connection and credentials by trying to remotely execute a simple "SELECT 1" query.
    void verifyConnection();

private:
    std::unordered_map<SQLHANDLE, std::shared_ptr<Descriptor>> descriptors;
    std::unordered_map<SQLHANDLE, std::shared_ptr<Statement>> statements;
};

template <> Descriptor& Connection::allocateChild<Descriptor>();
template <> void Connection::deallocateChild<Descriptor>(SQLHANDLE handle) noexcept;

template <> Statement& Connection::allocateChild<Statement>();
template <> void Connection::deallocateChild<Statement>(SQLHANDLE handle) noexcept;
