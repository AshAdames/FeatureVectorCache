#pragma once 
#include <iostream> 
#include <unordered_map> 
#include<string> 
#include <vector> 
#include <chrono>
#include <list> 

using namespace std; 


class KV{ 

    public:
        KV(int cap, std::chrono::seconds ttl);
        bool SET(string key, string val);
        string GET(string key);
        bool DEL(string key);
        ~KV();
        KV(const KV&) = delete;
        KV& operator=(const KV&) = delete;


    private:
        struct Node {
            string key;
            string val; 
            std::chrono::seconds TTL;
            chrono::steady_clock::time_point  expiration;
            Node(string k, string value, std::chrono::seconds ttl): key(k), val(value), expiration(chrono::steady_clock::now() + TTL) {}
        }; 

        //hashmap + DLL (list) for O(1) lookup + O(1) LRU replacement
        unordered_map<string, list<Node*>::iterator > cache; //k: key | val: ptr to value in list 
        list<Node *> valList; 
        size_t capacity; 
        std::chrono::seconds TTL;
};