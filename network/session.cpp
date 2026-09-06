
#include "session.hpp"

Session::Session(tcp::socket socket, KV_OPT& feat_store) 
    : socket_(std::move(socket)), feat_store_(feat_store) {}

// Session::Session(tcp::socket socket, KV_OPT& feat_store) 
//     : socket_(std::move(socket)), feat_store_(feat_store) {}

void Session::start(){
    do_read();
}


//protocols for feature vector kv
void Session::do_write(response resp){
    auto self = shared_from_this(); //keep session alive
    auto hdr = std::make_shared<msg_header>(resp.hdr);
    auto vec = std::make_shared<std::vector<float>>(std::move(resp.payload));


    //scatter gather vector of buffers [header, data]
    std::vector<asio::const_buffer> buffers = {
        asio::buffer(hdr.get(), sizeof(msg_header)), //header;
        asio::buffer(*vec)   //data    
    };

    asio::async_write(
        socket_,
        buffers,
        [this, self, hdr, vec](std::error_code ec, std::size_t bytes){
            if(!ec){
                std::cout<<"sending vec"; 
                do_read();
            }
        });
}


void Session::do_read(){
    auto self = shared_from_this(); 

    asio::async_read(
        socket_,
        asio::buffer(&incoming_hdr_, sizeof(msg_header)), //read header
        [this, self](std::error_code ec, std::size_t bytes){
            if(!ec){
            
                //payload with no vector (GET, DEL, SIM)
                if(incoming_hdr_.payload_len == 0){
                    auto resp = handle_command(incoming_hdr_, {}); 
                    do_write(resp);
                    return;
                }

                //payload with vector 
                incoming_vec_.resize(incoming_hdr_.payload_len); 
                std::size_t payload_bytes = incoming_hdr_.payload_len * sizeof(float); 

                asio::async_read(
                    socket_, 
                    asio::buffer(incoming_vec_.data(), payload_bytes),
                    [this, self](std::error_code ec, std::size_t) {
                        if(!ec){
                            //process vector
                            auto resp = handle_command(incoming_hdr_, incoming_vec_); 
                            do_write(resp);
                        }
                    });
            }
        });
}


Session::response Session::handle_command(msg_header & payload_hdr, const std::vector<float> &payload){
    //TODO switch to switch case 
    //TODO check empty cases for payload_hdr
    if(payload_hdr.cmd == 0){
        auto vec = feat_store_.GET(payload_hdr.key);
        response resp;
        resp.hdr.payload_len = vec.size();
        resp.status = !vec.empty();
        resp.payload = std::move(vec);

        return resp;


    }else if(payload_hdr.cmd == 1){
        bool ok = feat_store_.SET(payload_hdr.key, payload); 
        response resp;
        resp.payload = {}; 
        resp.hdr.payload_len = 0;
        resp.status = ok;
        return resp;

    }else if(payload_hdr.cmd == 2){
        bool ok = feat_store_.DEL(payload_hdr.key);
        response resp;
        resp.payload = {}; 
        resp.hdr.payload_len = 0;
        resp.status = ok;
        return resp;

    }else if(payload_hdr.cmd == 3){

        //this is placeholder as SIMILARITY is not yet completed
        auto pt = feat_store_.SIMILARITY(payload_hdr.key, payload_hdr.key2);
        if(pt.has_value()){
            response resp;
            resp.payload = {}; 
            resp.hdr.payload_len = 0;
            resp.sim = pt;
            resp.status = true;
            return resp;
        }
            response resp;
            resp.payload = {}; 
            resp.hdr.payload_len = 0;
            resp.status = false;
            return resp;

    }else if(payload_hdr.cmd == 4){
        auto sim_scores = feat_store_.TOPK(payload_hdr.key, payload_hdr.k); 
        response resp; resp.hdr = payload_hdr; 
        resp.hdr.payload_len = static_cast<int>(sim_scores.size());
        resp.sim_scores = sim_scores; 
        resp.status = true; 
        return resp;
    }

    response resp;
    resp.status = false; 
    return resp;
}


Session::~Session(){
    std::cout<<"deleted session object\n";
}

