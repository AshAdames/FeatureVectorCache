#pragma once 
#include <iostream> 
#include <unordered_map> 
#include<string> 
#include <vector> 
#include <chrono>
#include <list> 
#include <mutex>
#include <shared_mutex>

using namespace std; 


class KV{ 

    public:
        KV(int cap, int ttl);
        bool SET(const string& key, const string& val);
        string GET(const string& key);
        bool DEL(const string& key);
        ~KV();
        KV(const KV&) = delete;
        KV& operator=(const KV&) = delete;


    private:
        struct Node {
            string key;
            string val; 
            std::chrono::seconds TTL;
            chrono::steady_clock::time_point  expiration;
            Node(string k, string value, std::chrono::seconds ttl): key(k), val(value), expiration(chrono::steady_clock::now() + ttl) {}
        }; 

        //hashmap + DLL (list) for O(1) lookup + O(1) LRU replacement
        unordered_map<string, list<Node*>::iterator > cache; //k: key | val: ptr to value in list 
        list<Node *> valList; 
        size_t capacity; 
        std::chrono::seconds TTL;
        mutable std::shared_mutex sh_mtx;
        mutable std::mutex mtx;
};