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

    Server(asio::io_context& io, short port, KV_OPT& feat_store)
        : io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port)), feat_store_(feat_store) {
        do_accept_vec();
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

    //accept for feat_store
    void do_accept_vec() {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), feat_store_)->start();
                }
                do_accept_vec(); // re-arm — no recursion into run(), just registers next async op
            });
    }

    asio::io_context& io_;
    tcp::acceptor acceptor_;
    KV& store_;
    KV_OPT& feat_store_;
};