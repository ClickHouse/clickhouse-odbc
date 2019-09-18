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

/// Helper structure that represents information about where and
/// how to get or put values when reading or writing bound buffers.
struct BindingInfo {
    SQLSMALLINT type = SQL_C_DEFAULT;
    PTR value = nullptr;
    SQLLEN value_max_size = 0;
    SQLLEN * value_size_or_indicator = nullptr;
};

/// Helper structure that represents information about where and
/// how to get or put values when reading or writing bound parameter buffers.
struct ParamBindingInfo
    : public BindingInfo
{
    SQLSMALLINT io_type = SQL_PARAM_INPUT;
    SQLSMALLINT sql_type = SQL_UNKNOWN_TYPE;
};

/// Helper structure that represents different aspects of parameter info in a prepared query.
struct ParamInfo {
    std::string name;
};

class Statement
    : public Child<Connection, Statement>
{
private:
    using ChildType = Child<Connection, Statement>;

public:
    explicit Statement(Connection & connection);
    virtual ~Statement();

    /// Lookup TypeInfo for given name of type.
    const TypeInfo & getTypeInfo(const std::string & type_name, const std::string & type_name_without_parametrs = "") const;

    /// Prepare query for execution.
    void prepareQuery(const std::string & q);

    /// Execute previously prepared query.
    void executeQuery(IResultMutatorPtr && mutator = IResultMutatorPtr{});

    /// Prepare and execute query.
    void executeQuery(const std::string & q, IResultMutatorPtr && mutator = IResultMutatorPtr {});

    /// Indicates whether there is an result set available for reading.
    bool has_result_set() const;

    /// Make the next result set current, if any.
    bool advance_to_next_result_set();

    const ColumnInfo & getColumnInfo(size_t i) const;

    size_t getNumColumns() const;

    bool has_current_row() const;

    const Row & get_current_row() const;

    /// Checked way of retrieving the number of the current row in the current result set.
    std::size_t get_current_row_num() const;

    bool advance_to_next_row();

    /// Reset statement to initial state.
    void close_cursor();

    /// Reset/release row/column buffer bindings.
    void reset_col_bindings();

    /// Reset/release parameter buffer bindings.
    void reset_param_bindings();

    /// Access the effective descriptor by its role (type).
    Descriptor & get_effective_descriptor(SQLINTEGER type);

    /// Set an explicit descriptor for a role (type).
    void set_explicit_descriptor(SQLINTEGER type, std::shared_ptr<Descriptor> desc);

    /// Make an implicit descriptor active again.
    void set_implicit_descriptor(SQLINTEGER type);

private:
    void requestNextPackOfResultSets(IResultMutatorPtr && mutator);

    void processEscapeSequences();
    void extractParametersinfo();
    std::string buildFinalQuery(const std::vector<ParamBindingInfo>& param_bindings);
    std::vector<ParamBindingInfo> getParamsBindingInfo();

    Descriptor & choose(std::shared_ptr<Descriptor> & implicit_desc, std::weak_ptr<Descriptor> & explicit_desc);

    void allocate_implicit_descriptors();
    void deallocate_implicit_descriptors();

    std::shared_ptr<Descriptor> allocate_descriptor();
    void dellocate_descriptor(std::shared_ptr<Descriptor> & desc);

private:
    std::shared_ptr<Descriptor> implicit_ard;
    std::shared_ptr<Descriptor> implicit_apd;
    std::shared_ptr<Descriptor> implicit_ird;
    std::shared_ptr<Descriptor> implicit_ipd;

    std::weak_ptr<Descriptor> explicit_ard;
    std::weak_ptr<Descriptor> explicit_apd;
    std::weak_ptr<Descriptor> explicit_ird;
    std::weak_ptr<Descriptor> explicit_ipd;

    std::string query;
    std::vector<ParamInfo> parameters;

    std::unique_ptr<Poco::Net::HTTPResponse> response;
    std::istream* in = nullptr;
    std::unique_ptr<ResultSet> result_set;
    std::size_t next_param_set = 0;

public:
    // TODO: switch to using the corresponding descriptor attributes.
    std::map<SQLUSMALLINT, BindingInfo> bindings;
};
