#include "statement.h"
#include "escaping/escape_sequences.h"
#include "win/version.h"
#include "platform.h"
#include <Poco/Base64Encoder.h>
#include <Poco/Exception.h>
#include <Poco/Net/HTTPRequest.h>

Statement::Statement(Connection & conn_) : connection(conn_), metadata_id(conn_.environment.metadata_id) {
    ard.reset(new DescriptorClass);
    apd.reset(new DescriptorClass);
    ird.reset(new DescriptorClass);
    ipd.reset(new DescriptorClass);
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

const TypeInfo & Statement::getTypeInfo(const std::string & type_name) const {
    return connection.environment.types_info.at(type_name);
}

bool Statement::isEmpty() const {
    return query.empty();
}

bool Statement::isPrepared() const {
    return prepared;
}

void Statement::sendRequest(IResultMutatorPtr mutator) {
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
    request.setURI(
        "/?database=" + connection.getDatabase() + "&default_format=ODBCDriver2"); /// TODO Ability to transfer settings. TODO escaping

    request.set("User-Agent", "clickhouse-odbc/" VERSION_STRING " (" CMAKE_SYSTEM ")"
#if defined(UNICODE)
        " UNICODE"
#endif
    );

    LOG(request.getMethod() << " " << connection.session->getHost() << request.getURI() <<  " body=" << prepared_query);

    //LOG("curl 'http://" << connection.session->getHost() << ":" << connection.session->getPort() << request.getURI() << "' -d '" << prepared_query << "'");

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

void Statement::reset() {
    in = nullptr;
    response.reset();
    connection.session->reset();
    diagnostic_record.reset();
    result = ResultSet();

    ard.reset(new DescriptorClass);
    apd.reset(new DescriptorClass);
    ird.reset(new DescriptorClass);
    ipd.reset(new DescriptorClass);
}
