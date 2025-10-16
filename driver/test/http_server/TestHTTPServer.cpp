#include "TestHTTPServer.h"
#include <charconv>
#include <iostream>

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
    size_t header_size
        = co_await asio::async_read_until(socket, read_buffer, headers_terminator);

    // `async_read_until` often reads past the search marker, so we need to check how much it read beyond the headers
    size_t body_bytes_already_read = read_buffer.size() - header_size;

    std::istream stream(&read_buffer);
    auto content_size = read_content_length(stream);

    if (content_size != CHUNKED_ENCODED_CONTENT_LENGTH) {
        size_t bytes_remain = content_size - body_bytes_already_read;
        co_await asio::async_read(socket, read_buffer, asio::transfer_exactly(bytes_remain));
    } else {
        co_await asio::async_read_until(socket, read_buffer, chunk_encoding_terminator);
    }

    co_await asio::async_write(
        socket,
        asio::buffer(data));

    switch (keep_alive) {
        case KeepAlive::Drop:
            socket.set_option(asio::socket_base::linger{true, 0});
            socket.close();
            break;
        case KeepAlive::Close:
            socket.shutdown(tcp::socket::shutdown_both);
            break;
        case KeepAlive::KeepAlive:
            while (true) {
                auto [ec, ignore] = co_await asio::async_read(socket, read_buffer, asio::as_tuple);
                if (ec == asio::error::eof)
                    co_return;
                else if (ec)
                    throw std::system_error(ec);
            }
            break;
    }
}

asio::awaitable<void> TcpServer::listen()
{
    auto executor = co_await asio::this_coro::executor;
    tcp::endpoint endpoint{tcp::v4(), port};
    tcp::acceptor acceptor{executor, endpoint};

    for (;;) {
        auto socket = co_await acceptor.async_accept();
        co_await process_connection(std::move(socket));
    }
}

