#pragma once 
#include <iostream> 
#include <unordered_map> 
#include <string> 
#include <vector> 
#include <chrono>
#include <list> 
#include <mutex>
#include <shared_mutex>
#include <cmath>
#include <algorithm>
#include <optional>
#include <immintrin.h>
class KV_OPT{ 

    public:
        KV_OPT(int cap, int ttl, size_t dim);
        bool SET(const int& key, const std::vector<float>& vec);
        std::vector<float> GET(const int& key);
        bool DEL(const int& key);
        std::optional<float> SIMILARITY(int key1, int key2);
        std::vector<std::pair<int, float>> TOPK(int query_key, int k);
        ~KV_OPT();
        KV_OPT(const KV_OPT&) = delete;
        KV_OPT& operator=(const KV_OPT&) = delete;


    private:
        //nodes are for bookeeping, the real values will be in pool[]
        struct Node {
            int key;
            int prev = -1; 
            int next = -1; 
            float norm = 0.0f;
            std::chrono::steady_clock::time_point  expiration;
        };

        //hashmap + list for O(1) lookup + O(1) LRU replacement
        //cache =which slot, arena = data in that slot
        std::unordered_map<int, int> cache; //k: key | val: slot into pool/arena
        std::vector<Node> pool; // stores nodes 
        std::vector<float> arena; // continguous memory of dim * cap with only the raw float data 
            //arena provides cache locality and use for SIMD 
        size_t dim; // feature vector dim - fixed length of each vector
            //ex) dim = 4 == each vector is 4 floats 
        size_t capacity; // max size of  the KV_OPT 
        int head = -1;
        int tail = -1;
        //helper functions for adding/removing to linked list
        void unlink(int i);
        void push_front(int i);
        std::vector<int> free_slots;
        std::chrono::seconds TTL;
        mutable std::mutex mtx;
        mutable std::shared_mutex shd_mtx;

};