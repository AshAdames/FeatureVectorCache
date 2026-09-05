#pragma once
#include "kv.h"
#include "network/session.hpp"
#include <asio.hpp>
#include <memory>

using asio::ip::tcp;

class Server {
public:
    Server(asio::io_context& io, short port, KV& store)
        : io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port)), store_(store) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), store_)->start();
                }
                do_accept(); // re-arm — no recursion into run(), just registers next async op
            });
    }

    asio::io_context& io_;
    tcp::acceptor acceptor_;
    KV& store_;
};