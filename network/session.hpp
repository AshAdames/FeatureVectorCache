
#pragma once
#include <string>
#include <iostream> 
#include <asio.hpp>
#include <memory>
#include <array>
#include <utility>
#include <vector>
#include "kv.h"
#include "kv_opt.h"
using namespace asio;
using asio::ip::tcp;



class Session : public std::enable_shared_from_this<Session>{
public:
    Session(tcp::socket socket, KV &key_store);
    //session for feat_vec
    Session(tcp::socket socket, KV_OPT &key_vec_store);

    void start();
    ~Session();
private:
    tcp::socket socket_;
    asio::streambuf read_buf_;
    asio::streambuf read_vec_;
    KV &key_store_;
    KV_OPT &feat_store_;

    void do_write(std::string msg);
    void do_read();
    std::string handle_command(const std::string& line); 

    //protocols for feature vector kv
    struct msg_header{
        string cmd; 
        int key;
        int key2 = -1; 
        int payload_len; 
    };

    struct response {
        msg_header hdr;              // status/cmd echo + payload_len
        std::vector<float> payload; // empty if none
        optional<float> sim;
        bool status;
    };

    void do_write_vec(response resp); 
    void do_read_vec();
    response handle_command_vec(msg_header &payload_hdr, const std::vector<float> &payload);
    msg_header incoming_hdr_;
    response resp;
    std::vector<float> incoming_vec_;

};