#include "TestHTTPServer.h"
#include <charconv>
#include <iostream>
#include <sstream>

namespace {

constexpr size_t CHUNKED_ENCODED_CONTENT_LENGTH = SIZE_MAX;

size_t read_content_length(std::istream & stream)
{
    std::string header_line{};
    std::getline(stream, header_line);
    if (header_line.starts_with("GET")) {
        return 0;
    }

    if (header_line.starts_with("POST")) {
        while(!stream.eof()) {
            std::getline(stream, header_line);
            if (header_line.starts_with("Content-Length")) {
                auto size_begin = std::find(header_line.crbegin(), header_line.crend(), ' ');
                size_t content_length = 0;
                auto begin = &*size_begin.base();
                auto end = &*header_line.end() - 1;
                auto [ignore, ec] = std::from_chars(begin, end, content_length);
                if (ec != std::errc{})
                    throw std::runtime_error("failed to parse content length");
                return content_length;
            }
            else if (header_line == "\r") {
                // POST without content-length - assume chunk encoded
                return CHUNKED_ENCODED_CONTENT_LENGTH;
            }
        }
        throw std::runtime_error("header is not complete");
    }

    std::string method{header_line.data(), header_line.find(' ')};
    throw std::runtime_error("method " + method + "is not supported");
}

} // anonymous namespace

TcpServer::~TcpServer()
{
    stop();
}

asio::awaitable<void> TcpServer::start()
{
    try {
        co_await listen();
    } catch(std::exception & ex) {
        std::cerr << "ERROR: " << ex.what() << "\n";
    }
}

void TcpServer::stop()
{
    if (!io_context.stopped())
        io_context.stop();

    if (thread.joinable())
        thread.join();

    io_context.restart();
}

asio::awaitable<void> TcpServer::process_connection(tcp::socket socket)
{
    static const std::string_view headers_terminator{"\r\n\r\n"};
    static const std::string_view chunk_encoding_terminator{"0\r\n\r\n"};

    asio::streambuf read_buffer{};
    for (;;) {
        auto [ec, header_size]
            = co_await asio::async_read_until(socket, read_buffer, headers_terminator, asio::as_tuple);
        if (ec == asio::error::eof || ec == asio::error::connection_reset)
            co_return;
        if (ec)
            throw std::system_error(ec);

        // Consume all headers, preserving any body bytes read along with them.
        std::string headers(header_size, '\0');
        std::istream stream(&read_buffer);
        stream.read(headers.data(), headers.size());
        std::istringstream header_stream(headers);
        const auto content_size = read_content_length(header_stream);

        if (content_size != CHUNKED_ENCODED_CONTENT_LENGTH) {
            if (read_buffer.size() < content_size)
                co_await asio::async_read(socket, read_buffer, asio::transfer_exactly(content_size - read_buffer.size()));
            read_buffer.consume(content_size);
        } else {
            const auto body_size = co_await asio::async_read_until(socket, read_buffer, chunk_encoding_terminator);
            read_buffer.consume(body_size);
        }

        std::vector<char> response;
        KeepAlive response_keep_alive;
        {
            std::lock_guard lock(response_mutex);
            response = data;
            response_keep_alive = keep_alive;
        }
        auto [write_error, bytes_written] = co_await asio::async_write(socket, asio::buffer(response), asio::as_tuple);
        // Closing an unread cursor can disconnect while the response is still being sent.
        if (write_error == asio::error::connection_reset || write_error == asio::error::broken_pipe)
            co_return;
        if (write_error)
            throw std::system_error(write_error);

        switch (response_keep_alive) {
            case KeepAlive::Drop:
                socket.set_option(asio::socket_base::linger{true, 0});
                socket.close();
                co_return;
            case KeepAlive::Close:
                socket.shutdown(tcp::socket::shutdown_both);
                co_return;
            case KeepAlive::KeepAlive:
                break;
        }
    }
}

asio::awaitable<void> TcpServer::listen()
{
    // Connections are served one at a time: the next accept happens only after the
    // current connection is closed. Tests rely on the driver closing its socket
    // before it reconnects.
    for (;;) {
        auto socket = co_await acceptor.async_accept();
        ++connection_count;
        co_await process_connection(std::move(socket));
    }
}
