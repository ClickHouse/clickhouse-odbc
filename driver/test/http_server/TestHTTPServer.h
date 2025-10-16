#pragma once

#include <asio.hpp>

namespace ip = asio::ip;
using tcp = ip::tcp;

class TcpServer
{

public:
    enum class KeepAlive {
        KeepAlive,
        Close,
        Drop
    };

    TcpServer(ip::port_type port)
        : io_context{1}, port{port} {
      thread = std::thread{[this]() {
        asio::co_spawn(io_context, start(), asio::detached);
        io_context.run();
      }};
    }

    TcpServer(const TcpServer &) = delete;
    TcpServer(TcpServer &&) = delete;
    TcpServer &operator=(const TcpServer &) = delete;
    TcpServer &operator=(TcpServer &&) = delete;

    ~TcpServer();

    void setResponse(std::vector<char> data_) {
        data = std::move(data_);
    }

    void setKeepAlive(KeepAlive keep_alive_) {
        keep_alive = keep_alive_;
    }

    void stop();

private:

    asio::awaitable<void> start();
    asio::awaitable<void> listen();
    asio::awaitable<void> process_connection(tcp::socket socket);

    asio::io_context io_context;
    ip::port_type port;

    KeepAlive keep_alive{KeepAlive::Close};
    std::vector<char> data{};
    std::thread thread{};
};
