#pragma once

#include "connection.h"
#include "result_set.h"

#include <Poco/Base64Encoder.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>

#include <sstream>
#include <memory>

/// Information where and how to add values when reading.
struct Binding
{
    SQLSMALLINT target_type;
    PTR out_value;
    SQLLEN out_value_max_size;
    SQLLEN * out_value_size_or_indicator;
};

struct DescriptorClass
{
};

class Statement
{
public:
    Statement(Connection & conn_);

    void sendRequest();

    /// Fetch next row.
    bool fetchRow();

    void reset();

    /// Lookup TypeInfo for given name of type.
    const TypeInfo & GetTypeInfo(const std::string & type_name) const;

    Connection & connection;
    std::string query;
    std::unique_ptr<Poco::Net::HTTPResponse> response;
    std::istream * in = nullptr;
    bool prepared = false;

    DiagnosticRecord diagnostic_record;

    std::unique_ptr<DescriptorClass> ard;
    std::unique_ptr<DescriptorClass> apd;
    std::unique_ptr<DescriptorClass> ird;
    std::unique_ptr<DescriptorClass> ipd;

    ResultSet result;
    Row current_row;

    std::map<SQLUSMALLINT, Binding> bindings;
};
