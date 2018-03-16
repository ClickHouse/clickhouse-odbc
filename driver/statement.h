#pragma once

#include "connection.h"
#include "result_set.h"

#include <Poco/Net/HTTPResponse.h>

#include <memory>
#include <sstream>

/// Information where and how to add values when reading.
struct Binding {
    SQLSMALLINT target_type;
    PTR out_value;
    SQLLEN out_value_max_size;
    SQLLEN * out_value_size_or_indicator;
};

struct DescriptorClass {};

class Statement {
public:
    Statement(Connection & conn_);

    /// Whether the driver should scan the SQL string for escape sequences or not.
    bool getScanEscapeSequences() const;

    /// Enable or disable scannign the SQL string for escape sequences.
    void setScanEscapeSequences(bool value);

    /// Returns current value of SQL_ATTR_METADATA_ID.
    SQLUINTEGER getMetadataId() const;

    /// Sets value of SQL_ATTR_METADATA_ID.
    void setMetadataId(SQLUINTEGER id);

    /// Returns original query.
    const std::string getQuery() const;

    /// Lookup TypeInfo for given name of type.
    const TypeInfo & getTypeInfo(const std::string & type_name) const;

    bool isEmpty() const;

    bool isPrepared() const;

    /// Fetch next row.
    bool fetchRow();

    /// Do all the necessary work for preparing the query.
    void prepareQuery(const std::string & q);

    /// Set query without preparation.
    void setQuery(const std::string & q);

    /// Reset statement to initial state.
    void reset();

    /// Send request to a server.
    void sendRequest(IResultMutatorPtr mutator = nullptr);

public:
    Connection & connection;

    ResultSet result;
    Row current_row;

    std::istream * in = nullptr;
    DiagnosticRecord diagnostic_record;

    std::unique_ptr<DescriptorClass> ard;
    std::unique_ptr<DescriptorClass> apd;
    std::unique_ptr<DescriptorClass> ird;
    std::unique_ptr<DescriptorClass> ipd;

    std::map<SQLUSMALLINT, Binding> bindings;

private:
    std::unique_ptr<Poco::Net::HTTPResponse> response;

    /// An SQLUINTEGER value that determines
    /// how the string arguments of catalog functions are treated.
    SQLUINTEGER metadata_id;

    std::string query;
    std::string prepared_query;
    bool prepared = false;
    bool scan_escape_sequences = true;
};
