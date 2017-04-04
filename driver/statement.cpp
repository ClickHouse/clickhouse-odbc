#include "statement.h"

#include <Poco/Exception.h>

Statement::Statement(Connection & conn_)
    : connection(conn_)
{
    ard.reset(new DescriptorClass);
    apd.reset(new DescriptorClass);
    ird.reset(new DescriptorClass);
    ipd.reset(new DescriptorClass);
}

void Statement::sendRequest()
{
    std::ostringstream user_password_base64;
    Poco::Base64Encoder base64_encoder(user_password_base64);
    base64_encoder << connection.user << ":" << connection.password;
    base64_encoder.close();

    Poco::Net::HTTPRequest request;

    request.setMethod(Poco::Net::HTTPRequest::HTTP_POST);
    request.setVersion(Poco::Net::HTTPRequest::HTTP_1_1);
    request.setKeepAlive(true);
    request.setChunkedTransferEncoding(true);
    request.setCredentials("Basic", user_password_base64.str());
    request.setURI("/?database=" + connection.database + "&default_format=ODBCDriver"); /// TODO Ability to transfer settings. TODO escaping

    if (in && in->peek() != EOF)
        connection.session.reset();
    
    // Send request to server with finite count of retries.
    for (int i = 1; ; ++i)
    {
        try
        {
            connection.session.sendRequest(request) << query;
            response = std::make_unique<Poco::Net::HTTPResponse>();
            in = &connection.session.receiveResponse(*response);
            break;
        }
        catch (const Poco::IOException&)
        {
            if (i > connection.retry_count)
            {
                throw;
            }
        }
    }
    
    Poco::Net::HTTPResponse::HTTPStatus status = response->getStatus();

    if (status != Poco::Net::HTTPResponse::HTTP_OK)
    {
        std::stringstream error_message;
        error_message
            << "HTTP status code: " << status << std::endl
            << "Received error:" << std::endl
            << in->rdbuf() << std::endl;

        throw std::runtime_error(error_message.str());
    }

    result.init(*this);
}

bool Statement::fetchRow()
{
    current_row = result.fetch();
    return current_row;
}

void Statement::reset()
{
    in = nullptr;
    response.reset();
    connection.session.reset();
    diagnostic_record.reset();
    result = ResultSet();

    ard.reset(new DescriptorClass);
    apd.reset(new DescriptorClass);
    ird.reset(new DescriptorClass);
    ipd.reset(new DescriptorClass);
}
