
#include "session.hpp"

Session::Session(tcp::socket socket, KV& key_store) 
    : socket_(std::move(socket)), key_store_(key_store) {}

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

Session::~Session(){
    std::cout<<"deleted session object\n";
}

