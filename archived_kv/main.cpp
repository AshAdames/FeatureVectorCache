#include "kv.h"
#include "network/session.hpp"
#include "network/server.hpp"
#include <thread>
#include <vector>
#include <mutex>
#include <shared_mutex>

using namespace asio;
using asio::ip::tcp;


void do_run(io_context &io){
        io.run();
    }

int main(int argc, char** argv){ 
    int threadCount = 5; //default
    if(argc > 1 ){
        threadCount = std::stoi(argv[1]);
        if(threadCount < 0){
            std::cerr <<"thread count must be >= 1 \n";
            return 1;
        }
    }
    io_context io;
    KV key_store(10,20);
    Server server(io, 8080, key_store);

    std::vector<std::thread> threads;
    for(int i = 0; i < threadCount; ++i){
        //function than arguement, if not pass a lamnda, io.run() is callable, threads doesnt work that way
        threads.emplace_back(do_run, std::ref(io)); //emplace_back forwards arguments directly to the containers constructor
            //std::ref makes it cipyable, since do_run needs a reference
    }

    //join? jthread? 
    //so the idea is multiple threads will run io.run() (not multiple threads on KV store)
    //all these threads will work on the same KV store, that in turn now has mutexes
    //then move to sharding
    
    for(int i = 0; i < threads.size(); i++){
        threads[i].join();
    }
    return 0; 
}