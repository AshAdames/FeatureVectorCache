
#pragma once
#include <string>
#include <iostream> 
#include <asio.hpp>
#include <memory>
#include <array>
#include <utility>
#include <vector>
#include "kv.h"
using namespace asio;
using asio::ip::tcp;



class Session : public std::enable_shared_from_this<Session>{
public:
    Session(tcp::socket socket, KV &key_store);

    void start();
    ~Session();
private:
    tcp::socket socket_;
    asio::streambuf read_buf_;
    KV &key_store_;

    void do_write(std::string msg);
    void do_read();
    std::string handle_command(const std::string& line); 
};