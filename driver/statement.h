#pragma once

#include "driver.h"
#include "connection.h"
#include "descriptor.h"
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
private:
    using ChildType = Child<Connection, Statement>;

public:
    explicit Statement(Connection & connection);
    virtual ~Statement();

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
    void close_cursor();

    /// Reset/release row/column buffer bindings.
    void reset_col_bindings();

    /// Reset/release parameter buffer bindings.
    void reset_param_bindings();

    /// Send request to a server.
    void sendRequest(IResultMutatorPtr mutator = nullptr);

    /// Actual ARD, APD, IRD, IPD observers.
    Descriptor& ard();
    Descriptor& apd();
    Descriptor& ird();
    Descriptor& ipd();

    /// Explicit ARD, APD, IRD, IPD setters.
    void set_ard(std::shared_ptr<Descriptor> desc);
    void set_apd(std::shared_ptr<Descriptor> desc);
    void set_ird(std::shared_ptr<Descriptor> desc);
    void set_ipd(std::shared_ptr<Descriptor> desc);

    /// Explicit ARD, APD, IRD, IPD resetters.
    void reset_ard();
    void reset_apd();
    void reset_ird();
    void reset_ipd();

    /// Initialize a descriptor as an ARD, APD, IRD, IPD.
    void init_as_ard(Descriptor& desc);
    void init_as_apd(Descriptor& desc);
    void init_as_ird(Descriptor& desc);
    void init_as_ipd(Descriptor& desc);

private:
    void reset_descriptors();
    void deallocate_implicit_descriptors();
    std::shared_ptr<Descriptor> allocate_descriptor();
    void dellocate_descriptor(std::shared_ptr<Descriptor>& desc);

public:
    ResultSet result;
    Row current_row;

    std::istream * in = nullptr;

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
