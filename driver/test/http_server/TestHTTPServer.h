#pragma once

#include <asio.hpp>
#include <atomic>
#include <mutex>

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
        : io_context{1}, acceptor{io_context, tcp::endpoint{ip::address_v4::loopback(), port}} {
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
        std::lock_guard lock(response_mutex);
        data = std::move(data_);
    }

    void setKeepAlive(KeepAlive keep_alive_) {
        std::lock_guard lock(response_mutex);
        keep_alive = keep_alive_;
    }

    size_t connectionCount() const {
        return connection_count.load();
    }

    // Raw header block (request line + headers, up to and including the
    // terminating empty line) of the most recently received request.
    std::string lastRequestHeaders() const {
        std::lock_guard lock(response_mutex);
        return last_request_headers;
    }

    ip::port_type port() const {
        return acceptor.local_endpoint().port();
    }

    void stop();

private:

    asio::awaitable<void> start();
    asio::awaitable<void> listen();
    asio::awaitable<void> process_connection(tcp::socket socket);

    asio::io_context io_context;
    tcp::acceptor acceptor;

    KeepAlive keep_alive{KeepAlive::Close};
    std::vector<char> data{};
    std::string last_request_headers{};
    mutable std::mutex response_mutex;
    std::atomic<size_t> connection_count{0};
    std::thread thread{};
};
