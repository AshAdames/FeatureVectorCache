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


class KV_OPT{ 

    public:
        KV_OPT(int cap, int ttl);
        bool SET(const int& key, const float& val);
        string GET(const int& key);
        bool DEL(const int& key);
        float KV_OPT::SIMILARITY(int key1, int key2);
        ~KV_OPT();
        KV_OPT(const KV_OPT&) = delete;
        KV_OPT& operator=(const KV_OPT&) = delete;


    private:
        //nodes are for bookeeping, the real values will be in pool[]
        struct Node {
            int key;
            int prev = -1; 
            int next = -1; 
            chrono::steady_clock::time_point  expiration;
            Node(int k, std::chrono::seconds ttl): key(k), expiration(chrono::steady_clock::now() + ttl) {}
        }; 

        //hashmap + list for O(1) lookup + O(1) LRU replacement
        //cache =which slot, arena = data in that slot
        unordered_map<int, int> cache; //k: key | val: clot into pool/arena
        std::vector<Node> pool; // stores nodes 
        std::vector<float> arena; // continguous memory of dim * cap with only the raw float data 
            //arena provides cache locality and use for SIMD 
        size_t dim; // feature vector dim - fixed length of each vector
            //ex) dim = 4 == each vector is 4 floats 
        size_t capacity; // max size of  the KV_OPT 
        int head = -1;
        int tail = -1;
        //helper functions for adding/removing to linked list
        void KV_OPT::unlink(int i);
        void KV_OPT::push_front(int i);
        std::vector<int> freeSlots;
        std::chrono::seconds TTL;
        mutable std::mutex mtx;
};