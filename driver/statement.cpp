#include "driver/platform/platform.h"
#include "driver/utils/utils.h"
#include "driver/escaping/lexer.h"
#include "driver/escaping/escape_sequences.h"
#include "driver/statement.h"

#include <Poco/Exception.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/URI.h>
#include <Poco/UUID.h>
#include <Poco/UUIDGenerator.h>

#include <cctype>
#include <cstdio>

Statement::Statement(Connection & connection)
    : ChildType(connection)
{
    allocateImplicitDescriptors();
}

Statement::~Statement() {
    deallocateImplicitDescriptors();
}

const TypeInfo & Statement::getTypeInfo(const std::string & type_name, const std::string & type_name_without_parametrs) const {
    return getParent().getParent().getTypeInfo(type_name, type_name_without_parametrs);
}

void Statement::prepareQuery(const std::string & q) {
    closeCursor();
    query = q;
    processEscapeSequences();
    extractParametersinfo();
    is_prepared = true;
}

bool Statement::isPrepared() const {
    return is_prepared;
}

bool Statement::isExecuted() const {
    return is_executed;
}

void Statement::executeQuery(IResultMutatorPtr && mutator) {
    if (!is_prepared)
        throw std::runtime_error("statement not prepared");

    if (is_executed && is_forward_executed) {
        is_forward_executed = false;
        return;
    }

    auto * param_set_processed_ptr = getEffectiveDescriptor(SQL_ATTR_IMP_PARAM_DESC).getAttrAs<SQLULEN *>(SQL_DESC_ROWS_PROCESSED_PTR, 0);
    if (param_set_processed_ptr)
        *param_set_processed_ptr = 0;

    next_param_set = 0;
    requestNextPackOfResultSets(std::move(mutator));
    is_executed = true;
}

void Statement::requestNextPackOfResultSets(IResultMutatorPtr && mutator) {
    result_set.reset();

    const auto param_set_array_size = getEffectiveDescriptor(SQL_ATTR_APP_PARAM_DESC).getAttrAs<SQLULEN>(SQL_DESC_ARRAY_SIZE, 1);
    if (next_param_set >= param_set_array_size)
        return;

    getDiagHeader().setAttr(SQL_DIAG_ROW_COUNT, 0);

    auto & connection = getParent();

    if (connection.session && response && in)
        if (!*in || in->peek() != EOF)
            connection.session->reset();

    Poco::URI uri(connection.url);

    if (connection.port != 0)
        uri.setPort(connection.port);

    if (!connection.server.empty())
        uri.setHost(connection.server);

    bool database_set = false;
    bool default_format_set = false;

    for (const auto& parameter : uri.getQueryParameters()) {
        if (Poco::UTF8::icompare(parameter.first, "default_format")) {
            default_format_set = true;
        }
        else if (Poco::UTF8::icompare(parameter.first, "database")) {
            database_set = true;
        }
    }

    if (!default_format_set)
        uri.addQueryParameter("default_format", connection.default_format);

    if (!database_set)
        uri.addQueryParameter("database", connection.database);

    const auto param_bindings = getParamsBindingInfo(next_param_set);

    for (std::size_t i = 0; i < parameters.size(); ++i) {
        std::string value;

        if (param_bindings.size() <= i) {
            value = "Null";
        }
        else {
            const auto & binding_info = param_bindings[i];

            if (!isInputParam(binding_info.io_type) || isStreamParam(binding_info.io_type))
                throw std::runtime_error("Unable to extract data from bound param buffer: param IO type is not supported");

            if (binding_info.value == nullptr)
                value = "Null";
            else
                value = readReadyDataTo<std::string>(binding_info);
        }

        const auto param_name = getParamFinalName(i);
        uri.addQueryParameter("param_" + param_name, value);
    }

    const auto prepared_query = buildFinalQuery(param_bindings);

    // TODO: set this only after this single query is fully fetched (when output parameter support is added)
    auto * param_set_processed_ptr = getEffectiveDescriptor(SQL_ATTR_IMP_PARAM_DESC).getAttrAs<SQLULEN *>(SQL_DESC_ROWS_PROCESSED_PTR, 0);
    if (param_set_processed_ptr)
        *param_set_processed_ptr = next_param_set;

    Poco::Net::HTTPRequest request;
    request.setMethod(Poco::Net::HTTPRequest::HTTP_POST);
    request.setVersion(Poco::Net::HTTPRequest::HTTP_1_1);
    request.setKeepAlive(true);
    request.setChunkedTransferEncoding(true);
    request.setCredentials("Basic", connection.buildCredentialsString());
    request.setURI(uri.getPathEtc());
    request.set("User-Agent", connection.buildUserAgentString());

    LOG(request.getMethod() << " " << connection.session->getHost() << request.getURI() << " body=" << prepared_query
                            << " UA=" << request.get("User-Agent"));

    // LOG("curl 'http://" << connection.session->getHost() << ":" << connection.session->getPort() << request.getURI() << "' -d '" << prepared_query << "'");

    int redirect_count = 0;
    // Send request to server with finite count of retries.
    for (int i = 1;; ++i) {
        try {
            for (; redirect_count < connection.redirect_limit; ++redirect_count) {
                connection.session->sendRequest(request) << prepared_query;
                response = std::make_unique<Poco::Net::HTTPResponse>();
                in = &connection.session->receiveResponse(*response);
                auto status = response->getStatus();
                if (status != Poco::Net::HTTPResponse::HTTP_PERMANENT_REDIRECT && status != Poco::Net::HTTPResponse::HTTP_TEMPORARY_REDIRECT) {
                    break;
                }
                connection.session->reset(); // reset keepalived connection
                auto newLocation = response->get("Location");
                LOG("Redirected to " << newLocation << ", redirect index=" << redirect_count + 1 << "/" << connection.redirect_limit);
                uri = newLocation; 
                connection.session->setHost(uri.getHost());
                connection.session->setPort(uri.getPort());
                request.setURI(uri.getPathEtc());
            }
            break;
        } catch (const Poco::IOException & e) {
            connection.session->reset(); // reset keepalived connection
            LOG("Http request try=" << i << "/" << connection.retry_count << " failed: " << e.what() << ": " << e.message());
            if (i > connection.retry_count)
                throw;
        }
    }

    Poco::Net::HTTPResponse::HTTPStatus status = response->getStatus();
    if (status != Poco::Net::HTTPResponse::HTTP_OK) {
        std::stringstream error_message;
        if (status == Poco::Net::HTTPResponse::HTTP_TEMPORARY_REDIRECT || status == Poco::Net::HTTPResponse::HTTP_PERMANENT_REDIRECT) {
            error_message << "Redirect count exceeded" << std::endl << "Redirect limit: " << connection.redirect_limit << std::endl;
        } else {
            error_message << "HTTP status code: " << status << std::endl << "Received error:" << std::endl << in->rdbuf() << std::endl;
        }
        LOG(error_message.str());
        throw std::runtime_error(error_message.str());
    }

    auto format = connection.default_format;

    if (response->has("X-ClickHouse-Format"))
        format = response->get("X-ClickHouse-Format");

    result_set = std::make_unique<ResultSet>(format, *in, std::move(mutator));

    ++next_param_set;
}

void Statement::processEscapeSequences() {
    if (getAttrAs<SQLULEN>(SQL_ATTR_NOSCAN, SQL_NOSCAN_OFF) != SQL_NOSCAN_ON)
        query = replaceEscapeSequences(query);
}

void Statement::extractParametersinfo() {
    auto & apd_desc = getEffectiveDescriptor(SQL_ATTR_APP_PARAM_DESC);
    auto & ipd_desc = getEffectiveDescriptor(SQL_ATTR_IMP_PARAM_DESC);

    const auto apd_record_count = apd_desc.getRecordCount();
    auto ipd_record_count = ipd_desc.getRecordCount();

    // Reset IPD records but preserve possible info set by SQLBindParameter.
    ipd_record_count = std::min(ipd_record_count, apd_record_count);
    ipd_desc.setAttr(SQL_DESC_COUNT, ipd_record_count);

    parameters.clear();

    // TODO: implement this all in an upgraded Lexer.

    Poco::UUIDGenerator uuid_gen;
    auto generate_placeholder = [&] () {
        std::string placeholder;
        do {
            const auto uuid = uuid_gen.createOne();
            placeholder = '@' + uuid.toString();
        } while (query.find(placeholder) != std::string::npos);
        return placeholder;
    };

    // Replace all unquoted ? characters with a placeholder and populate 'parameters' array.
    char quoted_by = '\0';
    for (std::size_t i = 0; i < query.size(); ++i) {
        const char curr = query[i];
        const char next = (i < query.size() ? query[i + 1] : '\0');

        switch (curr) {
            case '\\': {
                ++i; // Skip the next char unconditionally.
                break;
            }

            case '"':
            case '\'': {
                if (quoted_by == curr) {
                    if (next == curr) {
                        ++i; // Skip the next char unconditionally: '' or "" SQL escaping.
                        break;
                    }
                    else {
                        quoted_by = '\0';
                    }
                }
                else {
                    quoted_by = curr;
                }
                break;
            }

            case '?': {
                if (quoted_by == '\0') {
                    ParamInfo param_info;
                    param_info.tmp_placeholder = generate_placeholder();
                    query.replace(i, 1, param_info.tmp_placeholder);
                    i += param_info.tmp_placeholder.size() - 1; // - 1 to compensate for's next ++i
                    parameters.emplace_back(param_info);
                }
                break;
            }

            case '@': {
                if (quoted_by == '\0') {
                    ParamInfo param_info;

                    param_info.name = '@';
                    for (std::size_t j = i + 1; j < query.size(); ++j) {
                        const char jcurr = query[j];
                        if (
                            jcurr == '_' ||
                            std::isalpha(jcurr) ||
                            (std::isdigit(jcurr) && j > i + 1)
                        ) {
                            param_info.name += jcurr;
                        }
                        else {
                            break;
                        }
                    }

                    if (param_info.name.size() == 1)
                        throw SqlException("Syntax error or access violation", "42000");

                    param_info.tmp_placeholder = generate_placeholder();
                    query.replace(i, param_info.name.size(), param_info.tmp_placeholder);
                    i += param_info.tmp_placeholder.size() - 1; // - 1 to compensate for's next ++i
                    parameters.emplace_back(param_info);
                }
                break;
            }
        }
    }

    ipd_record_count = std::max(ipd_record_count, parameters.size());
    ipd_desc.setAttr(SQL_DESC_COUNT, ipd_record_count);
}

std::string Statement::buildFinalQuery(const std::vector<ParamBindingInfo>& param_bindings) {
    auto prepared_query = query;

    for (std::size_t i = 0; i < parameters.size(); ++i) {
        const auto & param_info = parameters[i];
        std::string param_type;

        if (param_bindings.size() <= i) {
            param_type = "Nullable(Nothing)";
        }
        else {
            const auto & binding_info = param_bindings[i];

            BoundTypeInfo type_info;
            type_info.c_type = binding_info.c_type;
            type_info.sql_type = binding_info.sql_type;
            type_info.value_max_size = binding_info.value_max_size;
            type_info.precision = binding_info.precision;
            type_info.scale = binding_info.scale;
            type_info.is_nullable = (binding_info.is_nullable || binding_info.value == nullptr);

            param_type = convertSQLOrCTypeToDataSourceType(type_info);
        }

        const auto pos = prepared_query.find(param_info.tmp_placeholder);
        if (pos == std::string::npos)
            throw SqlException("COUNT field incorrect", "07002");

        const auto param_name = getParamFinalName(i);
        const std::string param_placeholder = "{" + param_name + ":" + param_type + "}";
        prepared_query.replace(pos, param_info.tmp_placeholder.size(), param_placeholder);
    }

    return prepared_query;
}

void Statement::executeQuery(const std::string & q, IResultMutatorPtr && mutator) {
    prepareQuery(q);
    executeQuery(std::move(mutator));
}

void Statement::forwardExecuteQuery(IResultMutatorPtr && mutator) {
    if (!is_prepared)
        throw std::runtime_error("statement not prepared");

    if (is_executed)
        return;

    executeQuery(std::move(mutator));
    is_forward_executed = true;
}

bool Statement::hasResultSet() const {
    return (isExecuted() && result_set);
}

bool Statement::advanceToNextResultSet() {
    if (!isExecuted())
        return false;

    getDiagHeader().setAttr(SQL_DIAG_ROW_COUNT, 0);

    IResultMutatorPtr mutator;

    if (hasResultSet())
        mutator = result_set->releaseMutator();

    // TODO: add support of detecting next result set on the wire, when protocol allows it.

    requestNextPackOfResultSets(std::move(mutator));
    return hasResultSet();
}

const ColumnInfo & Statement::getColumnInfo(size_t i) const {
    return result_set->getColumnInfo(i);
}

size_t Statement::getNumColumns() const {
    return (hasResultSet() ? result_set->getNumColumns() : 0);
}

bool Statement::hasCurrentRow() const {
    return (hasResultSet() ? result_set->hasCurrentRow() : false);
}

const Row & Statement::getCurrentRow() const {
    return result_set->getCurrentRow();
}

std::size_t Statement::getCurrentRowNum() const {
    return (hasResultSet() ? result_set->getCurrentRowNum() : 0);
}

bool Statement::advanceToNextRow() {
    bool advanced = false;

    if (hasResultSet()) {
        advanced = result_set->advanceToNextRow();
        if (!advanced)
            getDiagHeader().setAttr(SQL_DIAG_ROW_COUNT, result_set->getCurrentRowNum());
    }

    return advanced;
}

void Statement::closeCursor() {
    auto & connection = getParent();
    if (connection.session && response && in)
        if (!*in || in->peek() != EOF)
            connection.session->reset();

    result_set.reset();
    in = nullptr;
    response.reset();

    parameters.clear();
    query.clear();
    is_executed = false;
    is_forward_executed = false;
    is_prepared = false;
}

void Statement::resetColBindings() {
    bindings.clear();

    getEffectiveDescriptor(SQL_ATTR_APP_ROW_DESC).setAttr(SQL_DESC_COUNT, 0);
}

void Statement::resetParamBindings() {
    getEffectiveDescriptor(SQL_ATTR_APP_PARAM_DESC).setAttr(SQL_DESC_COUNT, 0);
}

std::string Statement::getParamFinalName(std::size_t param_idx) {
    auto & ipd_desc = getEffectiveDescriptor(SQL_ATTR_IMP_PARAM_DESC);
    if (param_idx < ipd_desc.getRecordCount()) {
        auto & ipd_record = ipd_desc.getRecord(param_idx + 1, SQL_ATTR_IMP_PARAM_DESC);
        if (ipd_record.getAttrAs<SQLSMALLINT>(SQL_DESC_UNNAMED, SQL_UNNAMED) != SQL_UNNAMED)
            return tryStripParamPrefix(ipd_record.getAttrAs<std::string>(SQL_DESC_NAME));
    }

    if (param_idx < parameters.size() && !parameters[param_idx].name.empty())
        return tryStripParamPrefix(parameters[param_idx].name);

    return "odbc_positional_" + std::to_string(param_idx + 1);
}

std::vector<ParamBindingInfo> Statement::getParamsBindingInfo(std::size_t param_set_idx) {
    std::vector<ParamBindingInfo> param_bindings;

    auto & apd_desc = getEffectiveDescriptor(SQL_ATTR_APP_PARAM_DESC);
    auto & ipd_desc = getEffectiveDescriptor(SQL_ATTR_IMP_PARAM_DESC);

    const auto apd_record_count = apd_desc.getRecordCount();
    const auto ipd_record_count = ipd_desc.getRecordCount();

    const auto fully_bound_param_count = std::min(apd_record_count, ipd_record_count);

    // We allow (apd_record_count < ipd_record_count) here, since we will set
    // all unbound parameters to 'Null' and their types to 'Nullable(Nothing)'.

    if (fully_bound_param_count > 0)
        param_bindings.reserve(fully_bound_param_count);

    const auto single_set_struct_size = apd_desc.getAttrAs<SQLULEN>(SQL_DESC_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN);
    const auto * bind_offset_ptr = apd_desc.getAttrAs<SQLULEN *>(SQL_DESC_BIND_OFFSET_PTR, 0);
    const auto bind_offset = (bind_offset_ptr ? *bind_offset_ptr : 0);

    for (std::size_t i = 1; i <= fully_bound_param_count; ++i) {
        ParamBindingInfo binding_info;

        auto & apd_record = apd_desc.getRecord(i, SQL_ATTR_APP_PARAM_DESC);
        auto & ipd_record = ipd_desc.getRecord(i, SQL_ATTR_IMP_PARAM_DESC);

        const auto * data_ptr = apd_record.getAttrAs<SQLPOINTER>(SQL_DESC_DATA_PTR, 0);
        const auto * sz_ptr = apd_record.getAttrAs<SQLLEN *>(SQL_DESC_OCTET_LENGTH_PTR, 0);
        const auto * ind_ptr = apd_record.getAttrAs<SQLLEN *>(SQL_DESC_INDICATOR_PTR, 0);

        binding_info.io_type = ipd_record.getAttrAs<SQLSMALLINT>(SQL_DESC_PARAMETER_TYPE, SQL_PARAM_INPUT);
        binding_info.c_type = apd_record.getAttrAs<SQLSMALLINT>(SQL_DESC_CONCISE_TYPE, SQL_C_DEFAULT);
        binding_info.sql_type = ipd_record.getAttrAs<SQLSMALLINT>(SQL_DESC_CONCISE_TYPE, SQL_UNKNOWN_TYPE);
        binding_info.value_max_size = ipd_record.getAttrAs<SQLULEN>(SQL_DESC_LENGTH, 0); // TODO: or SQL_DESC_OCTET_LENGTH ?
        binding_info.value = (void *)(data_ptr ? ((char *)(data_ptr) + param_set_idx * single_set_struct_size + bind_offset) : 0);
        binding_info.value_size = (SQLLEN *)(sz_ptr ? ((char *)(sz_ptr) + param_set_idx * sizeof(SQLLEN) + bind_offset) : 0);
        binding_info.indicator = (SQLLEN *)(ind_ptr ? ((char *)(ind_ptr) + param_set_idx * sizeof(SQLLEN) + bind_offset) : 0);

        // TODO: always use SQL_NULLABLE as a default when https://github.com/ClickHouse/ClickHouse/issues/7488 is fixed.
        binding_info.is_nullable = (
            ipd_record.getAttrAs<SQLSMALLINT>(SQL_DESC_NULLABLE,
                (isMappedToStringDataSourceType(binding_info.sql_type, binding_info.c_type) ? SQL_NO_NULLS : SQL_NULLABLE)
            ) == SQL_NULLABLE
        );

        binding_info.scale = ipd_record.getAttrAs<SQLSMALLINT>(SQL_DESC_SCALE, 0);
        binding_info.precision = ipd_record.getAttrAs<SQLSMALLINT>(SQL_DESC_PRECISION,
            (binding_info.sql_type == SQL_DECIMAL || binding_info.sql_type == SQL_NUMERIC ? 38 : 0)
        );

        param_bindings.emplace_back(binding_info);
    }

    return param_bindings;
}

Descriptor& Statement::getEffectiveDescriptor(SQLINTEGER type) {
    switch (type) {
        case SQL_ATTR_APP_ROW_DESC:   return choose(implicit_ard, explicit_ard);
        case SQL_ATTR_APP_PARAM_DESC: return choose(implicit_apd, explicit_apd);
        case SQL_ATTR_IMP_ROW_DESC:   return choose(implicit_ird, explicit_ird);
        case SQL_ATTR_IMP_PARAM_DESC: return choose(implicit_ipd, explicit_ipd);
    }
    throw std::runtime_error("unknown descriptor type");
}

void Statement::setExplicitDescriptor(SQLINTEGER type, std::shared_ptr<Descriptor> desc) {
    switch (type) {
        case SQL_ATTR_APP_ROW_DESC:   explicit_ard = desc; return;
        case SQL_ATTR_APP_PARAM_DESC: explicit_apd = desc; return;
        case SQL_ATTR_IMP_ROW_DESC:   explicit_ird = desc; return;
        case SQL_ATTR_IMP_PARAM_DESC: explicit_ipd = desc; return;
    }
    throw std::runtime_error("unknown descriptor type");
}

void Statement::setImplicitDescriptor(SQLINTEGER type) {
    return setExplicitDescriptor(type, std::shared_ptr<Descriptor>{});
}

Descriptor & Statement::choose(
    std::shared_ptr<Descriptor> & implicit_desc,
    std::weak_ptr<Descriptor> & explicit_desc
) {
    auto desc = explicit_desc.lock();
    return (desc ? *desc : *implicit_desc);
}

void Statement::allocateImplicitDescriptors() {
    deallocateImplicitDescriptors();

    implicit_ard = allocateDescriptor();
    implicit_apd = allocateDescriptor();
    implicit_ird = allocateDescriptor();
    implicit_ipd = allocateDescriptor();

    getParent().initAsDesc(*implicit_ard, SQL_ATTR_APP_ROW_DESC);
    getParent().initAsDesc(*implicit_apd, SQL_ATTR_APP_PARAM_DESC);
    getParent().initAsDesc(*implicit_ird, SQL_ATTR_IMP_ROW_DESC);
    getParent().initAsDesc(*implicit_ipd, SQL_ATTR_IMP_PARAM_DESC);
}

void Statement::deallocateImplicitDescriptors() {
    deallocateDescriptor(implicit_ard);
    deallocateDescriptor(implicit_apd);
    deallocateDescriptor(implicit_ird);
    deallocateDescriptor(implicit_ipd);
}

std::shared_ptr<Descriptor> Statement::allocateDescriptor() {
    auto & desc = getParent().allocateChild<Descriptor>();
    return desc.shared_from_this();
}

void Statement::deallocateDescriptor(std::shared_ptr<Descriptor> & desc) {
    if (desc) {
        desc->deallocateSelf();
        desc.reset();
    }
}
