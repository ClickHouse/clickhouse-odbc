#pragma once

#include "driver/driver.h"
#include "driver/connection.h"
#include "driver/descriptor.h"
#include "driver/result_set.h"

#include <Poco/Net/HTTPResponse.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

class Statement
    : public Child<Connection, Statement>
{
private:
    using ChildType = Child<Connection, Statement>;

public:
    explicit Statement(Connection & connection);
    virtual ~Statement();

    /// Lookup TypeInfo for given name of type.
    const TypeInfo & getTypeInfo(const std::string & type_name, const std::string & type_name_without_parameters) const;

    bool isPrepared() const;

    bool isExecuted() const;

    /// Prepare query for execution.
    void prepareQuery(const std::string & q);

    /// Execute previously prepared query, or no-op if previous execution was done using forwardExecuteQuery().
    void executeQuery(std::unique_ptr<ResultMutator> && mutator = std::unique_ptr<ResultMutator>{});

    /// Prepare and execute query.
    void executeQuery(const std::string & q, std::unique_ptr<ResultMutator> && mutator = std::unique_ptr<ResultMutator> {});

    /// Execute previously prepared query.
    void forwardExecuteQuery(std::unique_ptr<ResultMutator> && mutator = std::unique_ptr<ResultMutator> {});

    /// Indicates whether there is an result set available for reading.
    bool hasResultSet() const;

    /// Get the current result set.
    ResultSet & getResultSet();

    /// Get the current query id, or an empty string if query id is not applicable or not available.
    const std::string & getQueryId() const;

    /// Get the current content encoding, or an empty string if content encoding was not defined
    const std::string & getContentEncoding() const;

    /// Make the next result set current, if any.
    bool advanceToNextResultSet();

    /// Reset statement to initial state.
    void closeCursor();

    /// Reset/release row/column buffer bindings.
    void resetColBindings();

    /// Reset/release parameter buffer bindings.
    void resetParamBindings();

    /// Access the effective descriptor by its role (type).
    Descriptor & getEffectiveDescriptor(SQLINTEGER type);

    /// Set an explicit descriptor for a role (type).
    void setExplicitDescriptor(SQLINTEGER type, std::shared_ptr<Descriptor> desc);

    /// Make an implicit descriptor active again.
    void setImplicitDescriptor(SQLINTEGER type);

    void resetColumnOffsets()
    {
        column_offsets = {};
    }

    size_t getColumnOffset(SQLUSMALLINT column)
    {
        return column_offsets[column];
    }

    void setColumnOffset(SQLUSMALLINT column, size_t offset)
    {
        column_offsets[column] = offset;
    }

public:
    // public only for the unit tests
    struct HttpRequestData {
        std::string query;
        std::map<std::string, std::string> params;
    };
    HttpRequestData prepareHttpRequest();

private:
    void requestNextPackOfResultSets(std::unique_ptr<ResultMutator> && mutator);

    void extractParametersinfo();
    std::string buildFinalQuery(const std::vector<ParamBindingInfo>& param_bindings);
    std::string getParamFinalName(std::size_t param_idx);
    std::vector<ParamBindingInfo> getParamsBindingInfo(std::size_t param_set_idx);

    Descriptor & choose(std::shared_ptr<Descriptor> & implicit_desc, std::weak_ptr<Descriptor> & explicit_desc);

    void allocateImplicitDescriptors();
    void deallocateImplicitDescriptors();

    std::shared_ptr<Descriptor> allocateDescriptor();
    void deallocateDescriptor(std::shared_ptr<Descriptor> & desc);

private:
    std::shared_ptr<Descriptor> implicit_ard;
    std::shared_ptr<Descriptor> implicit_apd;
    std::shared_ptr<Descriptor> implicit_ird;
    std::shared_ptr<Descriptor> implicit_ipd;

    std::weak_ptr<Descriptor> explicit_ard;
    std::weak_ptr<Descriptor> explicit_apd;
    std::weak_ptr<Descriptor> explicit_ird;
    std::weak_ptr<Descriptor> explicit_ipd;

    bool is_prepared = false;
    bool is_forward_executed = false;
    bool is_executed = false;
    std::string query;
    std::vector<ParamInfo> parameters;

    // SQLGetData allows reading data in parts, i.e. any subsequent call
    // to it will send the rest of the data, if it did not initially fit
    // into the buffer. This map holds the offsets.
    std::unordered_map<SQLUSMALLINT, size_t> column_offsets;

    std::unique_ptr<Poco::Net::HTTPResponse> response;
    std::istream* in = nullptr;
    std::unique_ptr<ResultReader> result_reader;
    std::string query_id;
    std::string content_encoding;
    std::size_t next_param_set_idx = 0;
};
