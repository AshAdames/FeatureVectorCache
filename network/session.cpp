
#include "session.hpp"

Session::Session(tcp::socket socket, KV& key_store) 
    : socket_(std::move(socket)), key_store_(key_store) {}

Session::Session(tcp::socket socket, KV_OPT& feat_store) 
    : socket_(std::move(socket)), feat_store_(feat_store) {}

void Session::start(){
    do_read();
}

void Session::do_write(std::string msg){
    auto self = shared_from_this(); //keep session alive during async operations
    
    auto message = std::make_shared<std::string>(std::move(msg));
   
    asio::async_write(
        socket_, 
        asio::buffer(*message),
        [this, self](std::error_code ec, std::size_t bytes){
            if(!ec){
                std::cout<<"send msg \n";
                do_read(); 
            }
    });
}

void Session::do_read(){
    auto self = shared_from_this();
    asio::async_read_until(
        socket_, 
        read_buf_, 
        '\n',
        [this, self](std::error_code ec, std::size_t bytes){
            if(!ec){
                std::istream is(&read_buf_);
                std::string line;
                std::getline(is, line);
                
                std::string response = handle_command(line); // → your KV store
                do_write(response);
            }
    });
}

std::string Session::handle_command(const std::string& line){
    std::istringstream iss(line);
    std::string cmd, key, value; 
    iss >>cmd >>key; 

    if(cmd == "GET"){
        auto val = key_store_.GET(key); 
        return val.empty() ? "(nil)\n" : (val + "\n");
    }else if (cmd == "SET"){
        iss>>value; 
        key_store_.SET(key, value); 
        return "SET OK\n"; 
    }else if (cmd == "DEL"){
        iss>>value;
        key_store_.DEL(key); 
        return "DEL OK\n";

    }

    return "ERROR unknown cmd given \n"; 
}


//protocols for feature vector kv
void Session::do_write_vec(response resp){
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
                do_read_vec();
            }
        });
}


void Session::do_read_vec(){
    auto self = shared_from_this(); 

    asio::async_read(
        socket_,
        asio::buffer(&incoming_hdr_, sizeof(msg_header)), //read header
        [this, self](std::error_code ec, std::size_t bytes){
            if(!ec){
            
                //payload with no vector (GET, DEL, SIM)
                if(incoming_hdr_.payload_len == 0){
                    auto resp = handle_command_vec(incoming_hdr_, {}); 
                            do_write_vec(resp);
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
                            auto resp = handle_command_vec(incoming_hdr_, incoming_vec_); 
                            do_write_vec(resp);
                        }
                    }
                )
            }
        });
}


Session::response Session::handle_command_vec(msg_header & payload_hdr, const std::vector<float> &payload){
    
    if(payload_hdr.cmd == "GET"){
        auto vec = feat_store_.GET(payload_hdr.key);
        response resp;
        resp.hdr.payload_len = payload.size();
        resp.payload = std::move(vec);
        resp.status = vec.empty() ? true : false;
        return resp;


    }else if(payload_hdr.cmd == "SET"){
        bool ok = feat_store_.SET(payload_hdr.key, payload); 
        response resp;
        resp.payload = {}; 
        resp.hdr.payload_len = 0;
        resp.status = ok;
        return resp;

    }else if(payload_hdr.cmd == "DEL"){
        bool ok = feat_store_.DEL(payload_hdr.key);
        response resp;
        resp.payload = {}; 
        resp.hdr.payload_len = 0;
        resp.status = ok;
        return resp;

    }else if(payload_hdr.cmd == "SIMILIARITY"){

        //this is placeholder as SIMILARITY is not yet completed
        auto pt = feat_store_.SIMILARITY(payload_hdr.key, payload_hdr.key2);
        if(pt.has_value()){
            response resp;
            resp.payload = {}; 
            resp.hdr.payload_len = 0;
            resp.sim = pt;
            resp.status = true;
        }
            response resp;
            resp.payload = {}; 
            resp.hdr.payload_len = 0;
            resp.status = false;
        
        return resp;
    }
}


Session::~Session(){
    std::cout<<"deleted session object\n";
}

