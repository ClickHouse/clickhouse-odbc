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
    , metadata_id(connection.get_parent().metadata_id)
{
}

Statement::~Statement() {
    deallocate_implicit_descriptors();
}

bool Statement::getScanEscapeSequences() const {
    return scan_escape_sequences;
}

void Statement::setScanEscapeSequences(bool value) {
    scan_escape_sequences = value;
}

SQLUINTEGER Statement::getMetadataId() const {
    return metadata_id;
}

void Statement::setMetadataId(SQLUINTEGER id) {
    metadata_id = id;
}

const std::string Statement::getQuery() const {
    return query;
}

const TypeInfo & Statement::getTypeInfo(const std::string & type_name, const std::string & type_name_without_parametrs) const {
    return get_parent().get_parent().getTypeInfo(type_name, type_name_without_parametrs);
}

bool Statement::isEmpty() const {
    return query.empty();
}

bool Statement::isPrepared() const {
    return prepared;
}

void Statement::sendRequest(IResultMutatorPtr mutator) {
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

bool Statement::fetchRow() {
    current_row = result.fetch();
    return current_row.isValid();
}

void Statement::prepareQuery(const std::string & q) {
    query = q;
    if (scan_escape_sequences) {
        prepared_query = replaceEscapeSequences(query);
    } else {
        prepared_query = q;
    }

    prepared = true;
}

void Statement::setQuery(const std::string & q) {
    query = q;
    prepared_query = q;
}

void Statement::close_cursor() {
    in = nullptr;
    response.reset();
    get_parent().session->reset();
    reset_statuses();
    reset_header();
    result = ResultSet();
    reset_descriptors();
}

void Statement::reset_col_bindings() {
    bindings.clear();
}

void Statement::reset_param_bindings() {
}

Descriptor& Statement::ard() {
    auto desc = explicit_ard.lock();
    if (!desc) {
        if (!implicit_ard) {
            implicit_ard = allocate_descriptor();
            init_as_ard(*implicit_ard);
        }

        desc = implicit_ard;
        explicit_ard = desc;
    }
    return *desc;
}

Descriptor& Statement::apd() {
    auto desc = explicit_apd.lock();
    if (!desc) {
        if (!implicit_apd) {
            implicit_apd = allocate_descriptor();
            init_as_apd(*implicit_apd);
        }

        desc = implicit_apd;
        explicit_apd = desc;
    }
    return *desc;
}

Descriptor& Statement::ird() {
    auto desc = explicit_ird.lock();
    if (!desc) {
        if (!implicit_ird) {
            implicit_ird = allocate_descriptor();
            init_as_ird(*implicit_ird);
        }

        desc = implicit_ird;
        explicit_ird = desc;
    }
    return *desc;
}

Descriptor& Statement::ipd() {
    auto desc = explicit_ipd.lock();
    if (!desc) {
        if (!implicit_ipd) {
            implicit_ipd = allocate_descriptor();
            init_as_ipd(*implicit_ipd);
        }

        desc = implicit_ipd;
        explicit_ipd = desc;
    }
    return *desc;
}

void Statement::set_ard(std::shared_ptr<Descriptor> desc) {
    explicit_ard = desc;
}

void Statement::set_apd(std::shared_ptr<Descriptor> desc) {
    explicit_apd = desc;
}

void Statement::set_ird(std::shared_ptr<Descriptor> desc) {
    explicit_ird = desc;
}

void Statement::set_ipd(std::shared_ptr<Descriptor> desc) {
    explicit_ipd = desc;
}

void Statement::reset_ard() {
    set_ard(std::shared_ptr<Descriptor>{});
}

void Statement::reset_apd() {
    set_apd(std::shared_ptr<Descriptor>{});
}

void Statement::reset_ird() {
    set_ird(std::shared_ptr<Descriptor>{});
}

void Statement::reset_ipd() {
    set_ipd(std::shared_ptr<Descriptor>{});
}

void Statement::init_as_ard(Descriptor& desc) {
    
}

void Statement::init_as_apd(Descriptor& desc) {

}

void Statement::init_as_ird(Descriptor& desc) {

}

void Statement::init_as_ipd(Descriptor& desc) {

}

void Statement::reset_descriptors() {
    explicit_ard.reset();
    explicit_apd.reset();
    explicit_ird.reset();
    explicit_ipd.reset();

    if (implicit_ard) init_as_ard(*implicit_ard);
    if (implicit_apd) init_as_apd(*implicit_apd);
    if (implicit_ird) init_as_ird(*implicit_ird);
    if (implicit_ipd) init_as_ipd(*implicit_ipd);
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

void Statement::dellocate_descriptor(std::shared_ptr<Descriptor>& desc) {
    if (desc) {
        desc->deallocate_self();
        desc.reset();
    }
}

