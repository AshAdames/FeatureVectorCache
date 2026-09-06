
#pragma once
#include <string>
#include <iostream> 
#include <asio.hpp>
#include <memory>
#include <array>
#include <utility>
#include <vector>
#include "kv_opt.h"
using namespace asio;
using asio::ip::tcp;



class Session : public std::enable_shared_from_this<Session>{
public:
    Session(tcp::socket socket, KV_OPT &feat_store);

    void start();
    ~Session();
private:
    tcp::socket socket_;
    asio::streambuf read_vec_;
    KV_OPT &feat_store_;


    //protocols for feature vector kv
    /*
        cmd:
        0 = GET
        1 = SET
        2 = DEL
        3 = SIMILARITY
        4 = TOPK
    */

    //TODO rename and refactor
    struct msg_header{
        int cmd; 
        int key;
        int key2 = -1; 
        int k; 
        int payload_len; 
    };

    struct response {
        msg_header hdr;              // status/cmd echo + payload_len
        std::vector<float> payload; // empty if none
        std::optional<float> sim;
        std::vector<std::pair<int, float>> sim_scores;
        bool status;
    };

    void do_write(response resp); 
    void do_read();
    response handle_command(msg_header &payload_hdr, const std::vector<float> &payload);
    msg_header incoming_hdr_;
    std::vector<float> incoming_vec_;

};