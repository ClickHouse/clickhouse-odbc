#include "platform.h"
#include "utils.h"
#include "statement.h"
#include "escaping/lexer.h"
#include "escaping/escape_sequences.h"

#include <Poco/Exception.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/URI.h>

namespace {

    template <typename T>
    struct to {
        template <typename F>
        static T from(const BindingInfo& binding_info) {
            throw std::runtime_error("Unable to extract data from bound buffer: conversion from source type to target type not supported");
        }
    };

    template <>
    struct to<std::string> {
        template <typename F>
        static std::string from(const BindingInfo& binding_info) {
            if (!binding_info.value)
                return std::string{};

           const auto ind = reinterpret_cast<intptr_t>(binding_info.value_size_or_indicator);
            switch (ind) {
                case 0:
                case SQL_NTS:
                    break;

                case SQL_NULL_DATA:
                    return std::string{};

                case SQL_DEFAULT_PARAM:
                    return std::to_string(F{});

                default:
                    if (ind == SQL_DATA_AT_EXEC || ind < 0)
                        throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
            }

            return std::to_string(*(F*)binding_info.value);
        }

        template <>
        std::string from<SQLCHAR *>(const BindingInfo& binding_info) {
            if (!binding_info.value)
                return std::string{};

            const auto ind = reinterpret_cast<intptr_t>(binding_info.value_size_or_indicator);
            const auto * cstr = reinterpret_cast<const char *>(binding_info.value);

            switch (ind) {
                case 0:
                case SQL_NTS:
                    return std::string{cstr};

                case SQL_NULL_DATA:
                case SQL_DEFAULT_PARAM:
                    return std::string{};

                default:
                    if (ind == SQL_DATA_AT_EXEC || ind < 0)
                        throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
            }

            const auto size = *reinterpret_cast<SQLLEN *>(binding_info.value_size_or_indicator);
            return std::string(cstr, size);
        }

        template <>
        std::string from<SQLWCHAR *>(const BindingInfo& binding_info) {
            if (!binding_info.value)
                return std::string{};

            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;

            const auto ind = reinterpret_cast<intptr_t>(binding_info.value_size_or_indicator);
            const auto * wcstr = reinterpret_cast<const wchar_t *>(binding_info.value);

            switch (ind) {
                case 0:
                case SQL_NTS:
                    return convert.to_bytes(wcstr);

                case SQL_NULL_DATA:
                case SQL_DEFAULT_PARAM:
                    return std::string{};

                default:
                    if (ind == SQL_DATA_AT_EXEC || ind < 0)
                        throw std::runtime_error("Unable to extract data from bound buffer: data-at-execution bindings not supported");
            }

            const auto size = *reinterpret_cast<SQLLEN *>(binding_info.value_size_or_indicator);
            const auto* wcstr_last = wcstr + size / sizeof(decltype(*wcstr));
            return convert.to_bytes(wcstr, wcstr_last);
        }
    };

    template <typename T>
    T read_ready_data_to(const BindingInfo& binding_info) {
        switch (binding_info.type) {
            case SQL_C_CHAR:        return to<T>::template from<SQLCHAR *    >(binding_info);
            case SQL_C_WCHAR:       return to<T>::template from<SQLWCHAR *   >(binding_info);
            case SQL_C_SSHORT:      return to<T>::template from<SQLSMALLINT  >(binding_info);
            case SQL_C_USHORT:      return to<T>::template from<SQLUSMALLINT >(binding_info);
            case SQL_C_SLONG:       return to<T>::template from<SQLINTEGER   >(binding_info);
            case SQL_C_ULONG:       return to<T>::template from<SQLUINTEGER  >(binding_info);
            case SQL_C_FLOAT:       return to<T>::template from<SQLREAL      >(binding_info);
            case SQL_C_DOUBLE:      return to<T>::template from<SQLDOUBLE    >(binding_info);
            case SQL_C_BIT:         return to<T>::template from<SQLCHAR      >(binding_info);
            case SQL_C_STINYINT:    return to<T>::template from<SQLSCHAR     >(binding_info);
            case SQL_C_UTINYINT:    return to<T>::template from<SQLCHAR      >(binding_info);
            case SQL_C_SBIGINT:     return to<T>::template from<SQLBIGINT    >(binding_info);
            case SQL_C_UBIGINT:     return to<T>::template from<SQLUBIGINT   >(binding_info);
            case SQL_C_BINARY:      return to<T>::template from<SQLCHAR *    >(binding_info);
//          case SQL_C_BOOKMARK:    return to<T>::template from<BOOKMARK     >(binding_info);
//          case SQL_C_VARBOOKMARK: return to<T>::template from<SQLCHAR *    >(binding_info);

            default:
                throw std::runtime_error("Unable to extract data from bound buffer: source type representation not supported");
        }
    }

} // namespace

Statement::Statement(Connection & connection)
    : ChildType(connection)
{
    allocate_implicit_descriptors();
}

Statement::~Statement() {
    deallocate_implicit_descriptors();
}

const TypeInfo & Statement::getTypeInfo(const std::string & type_name, const std::string & type_name_without_parametrs) const {
    return get_parent().get_parent().getTypeInfo(type_name, type_name_without_parametrs);
}

void Statement::prepareQuery(const std::string & q) {
    close_cursor();

    query = q;
    if (get_attr_as<SQLULEN>(SQL_ATTR_NOSCAN, SQL_NOSCAN_OFF) == SQL_NOSCAN_ON) {
        prepared_query = query;
    }
    else {
        prepared_query = replaceEscapeSequences(query);
    }
}

void Statement::executeQuery(IResultMutatorPtr && mutator) {
    auto * param_set_processed_ptr = get_effective_descriptor(SQL_ATTR_IMP_PARAM_DESC).get_attr_as<SQLULEN *>(SQL_DESC_ROWS_PROCESSED_PTR, 0);
    if (param_set_processed_ptr)
        *param_set_processed_ptr = 0;

    next_param_set = 0;
    requestNextPackOfResultSets(std::move(mutator));
}

void Statement::requestNextPackOfResultSets(IResultMutatorPtr && mutator) {
    const auto param_set_array_size = get_effective_descriptor(SQL_ATTR_APP_PARAM_DESC).get_attr_as<SQLULEN>(SQL_DESC_ARRAY_SIZE, 1);
    if (next_param_set >= param_set_array_size)
        return;

    get_diag_header().set_attr(SQL_DIAG_ROW_COUNT, 0);

    auto & connection = get_parent();

    if (connection.session && response && in)
        if (!*in || in->peek() != EOF)
            connection.session->reset();

    Poco::URI uri(connection.url);
    uri.addQueryParameter("database", connection.getDatabase());
    uri.addQueryParameter("default_format", "ODBCDriver2");

    for (std::size_t i = 0; i < parameters.size(); ++i) {
        const auto & param_info = parameters[i];
        const auto binding_info = get_param_binding_info(i);

        if (!is_input_param(binding_info.io_type) || is_stream_param(binding_info.io_type))
            throw std::runtime_error("Unable to extract data from bound param buffer: param IO type is not supported");

        uri.addQueryParameter("param_" + param_info.name, read_ready_data_to<std::string>(binding_info));
    }

    // TODO: set this only after this single query is fully fetched (when output parameter support is added)
    auto * param_set_processed_ptr = get_effective_descriptor(SQL_ATTR_IMP_PARAM_DESC).get_attr_as<SQLULEN *>(SQL_DESC_ROWS_PROCESSED_PTR, 0);
    if (param_set_processed_ptr)
        *param_set_processed_ptr = next_param_set;

    Poco::Net::HTTPRequest request;
    request.setMethod(Poco::Net::HTTPRequest::HTTP_POST);
    request.setVersion(Poco::Net::HTTPRequest::HTTP_1_1);
    request.setKeepAlive(true);
    request.setChunkedTransferEncoding(true);
    request.setCredentials("Basic", connection.buildCredentialsString());
    request.setURI(uri.toString());
    request.set("User-Agent", connection.buildUserAgentString());

    LOG(request.getMethod() << " " << connection.session->getHost() << request.getURI() << " body=" << prepared_query
                            << " UA=" << request.get("User-Agent"));

    // LOG("curl 'http://" << connection.session->getHost() << ":" << connection.session->getPort() << request.getURI() << "' -d '" << prepared_query << "'");

    // Send request to server with finite count of retries.
    for (int i = 1;; ++i) {
        try {
            connection.session->sendRequest(request) << prepared_query;
            response = std::make_unique<Poco::Net::HTTPResponse>();
            in = &connection.session->receiveResponse(*response);
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
        error_message << "HTTP status code: " << status << std::endl << "Received error:" << std::endl << in->rdbuf() << std::endl;
        LOG(error_message.str());
        throw std::runtime_error(error_message.str());
    }

    result_set.reset(new ResultSet{*in, std::move(mutator)});

    ++next_param_set;
}

void Statement::executeQuery(const std::string & q, IResultMutatorPtr && mutator) {
    prepareQuery(q);
    executeQuery(std::move(mutator));
}

bool Statement::has_result_set() const {
    return !!result_set;
}

bool Statement::advance_to_next_result_set() {
    get_diag_header().set_attr(SQL_DIAG_ROW_COUNT, 0);

    IResultMutatorPtr mutator;

    if (has_result_set())
        mutator = result_set->release_mutator();

    // TODO: add support of detecting next result set on the wire, when protocol allows it.
    result_set.reset();

    requestNextPackOfResultSets(std::move(mutator));
    return has_result_set();
}

const ColumnInfo & Statement::getColumnInfo(size_t i) const {
    return result_set->getColumnInfo(i);
}

size_t Statement::getNumColumns() const {
    return (has_result_set() ? result_set->getNumColumns() : 0);
}

bool Statement::has_current_row() const {
    return (has_result_set() ? result_set->has_current_row() : false);
}

const Row & Statement::get_current_row() const {
    return result_set->get_current_row();
}

std::size_t Statement::get_current_row_num() const {
    return (has_result_set() ? result_set->get_current_row_num() : 0);
}

bool Statement::advance_to_next_row() {
    bool advanced = false;

    if (has_result_set()) {
        advanced = result_set->advance_to_next_row();
        if (!advanced)
            get_diag_header().set_attr(SQL_DIAG_ROW_COUNT, result_set->get_current_row_num());
    }

    return advanced;
}

void Statement::close_cursor() {
    auto & connection = get_parent();
    if (connection.session && response && in)
        if (!*in || in->peek() != EOF)
            connection.session->reset();

    result_set.reset();
    in = nullptr;
    response.reset();

    parameters.clear();
    prepared_query.clear();
    query.clear();
}

void Statement::reset_col_bindings() {
    bindings.clear();
//  get_effective_descriptor(SQL_ATTR_APP_ROW_DESC).set_attr(SQL_DESC_COUNT, 0);
}

void Statement::reset_param_bindings() {
    get_effective_descriptor(SQL_ATTR_APP_PARAM_DESC).set_attr(SQL_DESC_COUNT, 0);
}

ParamBindingInfo Statement::get_param_binding_info(std::size_t i) {
    ParamBindingInfo binding_info;

    auto & apd = get_effective_descriptor(SQL_ATTR_APP_PARAM_DESC);
    auto & ipd = get_effective_descriptor(SQL_ATTR_IMP_PARAM_DESC);

    const auto single_set_struct_size = apd.get_attr_as<SQLULEN>(SQL_DESC_BIND_TYPE, SQL_PARAM_BIND_BY_COLUMN);
    const auto * bind_offset_ptr = apd.get_attr_as<SQLULEN *>(SQL_DESC_BIND_OFFSET_PTR, 0);

    const auto bind_offset = (bind_offset_ptr ? *bind_offset_ptr : 0);

    const auto * data_ptr = apd.get_attr_as<SQLPOINTER>(SQL_DESC_DATA_PTR, 0);
    const auto * ind_ptr = apd.get_attr_as<SQLLEN *>(SQL_DESC_INDICATOR_PTR, 0);

    binding_info.io_type = ipd.get_attr_as<SQLSMALLINT>(SQL_DESC_PARAMETER_TYPE, SQL_PARAM_INPUT);
    binding_info.type = apd.get_attr_as<SQLSMALLINT>(SQL_DESC_CONCISE_TYPE, SQL_C_DEFAULT);
    binding_info.sql_type = ipd.get_attr_as<SQLSMALLINT>(SQL_DESC_CONCISE_TYPE, SQL_UNKNOWN_TYPE);
    binding_info.value = (void *)(data_ptr ? ((char *)(data_ptr) + i * single_set_struct_size + bind_offset) : 0);
    binding_info.value_size_or_indicator = (SQLLEN *)(ind_ptr ? ((char *)(ind_ptr) + i * sizeof(SQLLEN) + bind_offset) : 0);

    return binding_info;
}

Descriptor& Statement::get_effective_descriptor(SQLINTEGER type) {
    switch (type) {
        case SQL_ATTR_APP_ROW_DESC:   return choose(implicit_ard, explicit_ard);
        case SQL_ATTR_APP_PARAM_DESC: return choose(implicit_apd, explicit_apd);
        case SQL_ATTR_IMP_ROW_DESC:   return choose(implicit_ird, explicit_ird);
        case SQL_ATTR_IMP_PARAM_DESC: return choose(implicit_ipd, explicit_ipd);
    }
    throw std::runtime_error("unknown descriptor type");
}

void Statement::set_explicit_descriptor(SQLINTEGER type, std::shared_ptr<Descriptor> desc) {
    switch (type) {
        case SQL_ATTR_APP_ROW_DESC:   explicit_ard = desc; return;
        case SQL_ATTR_APP_PARAM_DESC: explicit_apd = desc; return;
        case SQL_ATTR_IMP_ROW_DESC:   explicit_ird = desc; return;
        case SQL_ATTR_IMP_PARAM_DESC: explicit_ipd = desc; return;
    }
    throw std::runtime_error("unknown descriptor type");
}

void Statement::set_implicit_descriptor(SQLINTEGER type) {
    return set_explicit_descriptor(type, std::shared_ptr<Descriptor>{});
}

Descriptor & Statement::choose(
    std::shared_ptr<Descriptor> & implicit_desc,
    std::weak_ptr<Descriptor> & explicit_desc
) {
    auto desc = explicit_desc.lock();
    return (desc ? *desc : *implicit_desc);
}

void Statement::allocate_implicit_descriptors() {
    deallocate_implicit_descriptors();

    implicit_ard = allocate_descriptor();
    implicit_apd = allocate_descriptor();
    implicit_ird = allocate_descriptor();
    implicit_ipd = allocate_descriptor();

    get_parent().init_as_desc(*implicit_ard, SQL_ATTR_APP_ROW_DESC);
    get_parent().init_as_desc(*implicit_apd, SQL_ATTR_APP_PARAM_DESC);
    get_parent().init_as_desc(*implicit_ird, SQL_ATTR_IMP_ROW_DESC);
    get_parent().init_as_desc(*implicit_ipd, SQL_ATTR_IMP_PARAM_DESC);
}

void Statement::deallocate_implicit_descriptors() {
    dellocate_descriptor(implicit_ard);
    dellocate_descriptor(implicit_apd);
    dellocate_descriptor(implicit_ird);
    dellocate_descriptor(implicit_ipd);
}

std::shared_ptr<Descriptor> Statement::allocate_descriptor() {
    auto & desc = get_parent().allocate_child<Descriptor>();
    return desc.shared_from_this();
}

void Statement::dellocate_descriptor(std::shared_ptr<Descriptor> & desc) {
    if (desc) {
        desc->deallocate_self();
        desc.reset();
    }
}
