#pragma once

#include "driver.h"
#include "connection.h"
#include "descriptor.h"
#include "diagnostics.h"
#include "result_set.h"

#include <Poco/Net/HTTPResponse.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

/// Information where and how to add values when reading.
struct Binding {
    SQLSMALLINT target_type;
    PTR out_value;
    SQLLEN out_value_max_size;
    SQLLEN * out_value_size_or_indicator;
};

class Statement
    : public Child<Connection, Statement>
{
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
    const TypeInfo & getTypeInfo(const std::string & type_name, const std::string & type_name_without_parametrs = "") const;

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

    /// Actual ARD observer.
    Descriptor& ard();

    /// Actual APD observer.
    Descriptor& apd();

    /// Actual IRD observer.
    Descriptor& ird();

    /// Actual IPD observer.
    Descriptor& ipd();

    /// Explicit ARD setter.
    void set_ard(std::shared_ptr<Descriptor> desc);

    /// Explicit APD setter.
    void set_apd(std::shared_ptr<Descriptor> desc);

    /// Explicit IRD setter.
    void set_ird(std::shared_ptr<Descriptor> desc);

    /// Explicit IPD setter.
    void set_ipd(std::shared_ptr<Descriptor> desc);

    /// Default ARD factory.
    std::shared_ptr<Descriptor> make_ard();

    /// Default APD factory.
    std::shared_ptr<Descriptor> make_apd();

    /// Default IRD factory.
    std::shared_ptr<Descriptor> make_ird();

    /// Default IPD factory.
    std::shared_ptr<Descriptor> make_ipd();

public:
    Connection & connection;

    ResultSet result;
    Row current_row;

    std::istream * in = nullptr;
    DiagnosticRecord diagnostic_record;

    std::map<SQLUSMALLINT, Binding> bindings;

    SQLULEN * rows_fetched_ptr = nullptr;
    SQLULEN row_array_size = 1;

private:
    std::shared_ptr<Descriptor> implicit_ard;
    std::shared_ptr<Descriptor> implicit_apd;
    std::shared_ptr<Descriptor> implicit_ird;
    std::shared_ptr<Descriptor> implicit_ipd;

    std::weak_ptr<Descriptor> explicit_ard;
    std::weak_ptr<Descriptor> explicit_apd;
    std::weak_ptr<Descriptor> explicit_ird;
    std::weak_ptr<Descriptor> explicit_ipd;

    std::unique_ptr<Poco::Net::HTTPResponse> response;

    /// An SQLUINTEGER value that determines
    /// how the string arguments of catalog functions are treated.
    SQLUINTEGER metadata_id;

    std::string query;
    std::string prepared_query;
    bool prepared = false;
    bool scan_escape_sequences = true;
};
