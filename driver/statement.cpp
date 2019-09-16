#include "statement.h"
#include "escaping/lexer.h"
#include "escaping/escape_sequences.h"
#include "platform.h"
#include "win/version.h"

#include <Poco/Base64Encoder.h>
#include <Poco/Exception.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/URI.h>

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
    query = q;
    if (get_attr_as<SQLULEN>(SQL_ATTR_NOSCAN, SQL_NOSCAN_OFF) == SQL_NOSCAN_ON) {
        prepared_query = query;
    }
    else {
        prepared_query = replaceEscapeSequences(query);
    }
}

void Statement::executeQuery(IResultMutatorPtr mutator) {
    auto & connection = get_parent();

    std::ostringstream user_password_base64;
    Poco::Base64Encoder base64_encoder(user_password_base64, Poco::BASE64_URL_ENCODING);
    base64_encoder << connection.user << ":" << connection.password;
    base64_encoder.close();

    Poco::Net::HTTPRequest request;

    request.setMethod(Poco::Net::HTTPRequest::HTTP_POST);
    request.setVersion(Poco::Net::HTTPRequest::HTTP_1_1);
    request.setKeepAlive(true);
    request.setChunkedTransferEncoding(true);
    request.setCredentials("Basic", user_password_base64.str());
    Poco::URI uri(connection.url);
    uri.addQueryParameter("database", connection.getDatabase());
    uri.addQueryParameter("default_format", "ODBCDriver2");
    request.setURI(connection.path + "?" + uri.getQuery()); /// TODO escaping
    request.set("User-Agent",
        std::string {}
            + "clickhouse-odbc/" VERSION_STRING " (" CMAKE_SYSTEM ")"
#if defined(UNICODE)
              " UNICODE"
#endif
            + (connection.useragent.empty() ? "" : " " + connection.useragent));

    LOG(request.getMethod() << " " << connection.session->getHost() << request.getURI() << " body=" << prepared_query
                            << " UA=" << request.get("User-Agent"));

    // LOG("curl 'http://" << connection.session->getHost() << ":" << connection.session->getPort() << request.getURI() << "' -d '" << prepared_query << "'");

    if (in && in->peek() != EOF)
        connection.session->reset();
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
            if (i > connection.retry_count) {
                throw;
            }
        }
    }

    Poco::Net::HTTPResponse::HTTPStatus status = response->getStatus();

    if (status != Poco::Net::HTTPResponse::HTTP_OK) {
        std::stringstream error_message;
        error_message << "HTTP status code: " << status << std::endl << "Received error:" << std::endl << in->rdbuf() << std::endl;
        LOG(error_message.str());
        throw std::runtime_error(error_message.str());
    }

    result.init(this, std::move(mutator));
}

void Statement::executeQuery(const std::string & q, IResultMutatorPtr mutator) {
    prepareQuery(q);
    executeQuery(std::move(mutator));
}

bool Statement::fetchRow() {
    current_row = result.fetch();
    return current_row.isValid();
}

void Statement::close_cursor() {
    in = nullptr;
    response.reset();
    get_parent().session->reset();
    result = ResultSet();
    reset_diag();
}

void Statement::reset_col_bindings() {
    bindings.clear();
//  get_effective_descriptor(SQL_ATTR_APP_ROW_DESC).set_attr(SQL_DESC_COUNT, 0);
}

void Statement::reset_param_bindings() {
    get_effective_descriptor(SQL_ATTR_APP_PARAM_DESC).set_attr(SQL_DESC_COUNT, 0);
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
