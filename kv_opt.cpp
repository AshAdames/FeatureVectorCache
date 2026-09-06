#include "kv_opt.h"

/* TODO TODAY
- intrinsct list - fix Node 
- change val from string to fixed float 
- fix protocol for vector of floats
- implement SIMILARIRTY key1 key2 scalar cosine 
- implement TOPK
*/

/* General TODO 
- correctness tests 

- shard by key 
re benchmark
*/

//helper functions for adding/removing to linked list
//-1 means empty
void KV_OPT::unlink(int i){
    //removing from linked list, remove and relink neighbots (prev, next)
    int prev = pool[i].prev;
    int next = pool[i].next;
    // A -> i -> C , remove B
    if(prev != -1){
        pool[pool[i].prev].next = pool[i].next; //unlink it 
    }
    else{  //found head, no prev
        head = pool[i].next;
    } //head = -1
    
    if(pool[i].next != -1) {
        pool[pool[i].next].prev = pool[i].prev; //unlink
    } 
    else{//found tail, no next
        tail = pool[i].prev; 
    }

    pool[i].prev = pool[i].next = -1; //free index 

}



void KV_OPT::push_front(int i){
    pool[i].prev = -1; 
    pool[i].next = head; 
    if(head != -1) 
        pool[head].prev = i; 
    head = i; 
    if(tail == -1) 
        tail = i; //empty list 
}


//constructor 
KV_OPT::KV_OPT(int cap, int ttl, size_t dim){
    capacity = cap;
    this->dim = dim;
    arena.resize(cap*dim);
    pool.resize(cap);
    //default ttl for entire KV_OPT, in secs
    TTL = std::chrono::seconds(ttl);

    //push index onto freeSlots 
    freeSlots.resize(capacity); 
    for(int i = 0; i < capacity; ++i){
        freeSlots[i] = i; 
    }
}


//Set
bool KV_OPT::SET(const int& key, const std::vector<float>& vec){ 
    std::lock_guard<std::mutex> lock(mtx);
    if(vec.size() != dim) return false;
    //if freeSlots is non empty, pop off an index and use it 
    auto it = cache.find(key);
    if(it != cache.end()){ //key exists just update it 
        int i = it->second;
        std::copy(vec.begin(), vec.end(), arena.begin() + i * dim);
        unlink(i);
        push_front(i);
        return true;
    }

    int i;
    if(!freeSlots.empty()){//get index 
        i = freeSlots.back(); 
        freeSlots.pop_back();
    }else{//no free slots, get the tail index, free that slot, use that index 
        i = tail;
        cache.erase(pool[i].key);
        unlink(i);
    }

    //create new key,vec, push to front
    std::copy(vec.begin(), vec.end(), arena.begin() + i * dim);
    pool[i].key = key;
    pool[i].expiration = std::chrono::steady_clock::now() + TTL; 
    cache[key] = i; 
    push_front(i);
    return true;
}
   
//Get
std::vector<float> KV_OPT::GET(const int& key){
    //use hashmap to determine if it exists 
    std::lock_guard<std::mutex> lock(mtx);
    auto it = cache.find(key);
    if(it == cache.end()) return {};

    int i = it->second; 
    if(std::chrono::steady_clock::now() > pool[i].expiration){
        //laxy exp check
        cache.erase(it); 
        unlink(i); 
        freeSlots.push_back(i); 
        return {};
    }

    //update lru
    unlink(i);
    push_front(i);
    //return feature vector, which is slot in arena 
    return std::vector<float>(arena.begin() + i*dim, arena.begin() + i*dim + dim);
}

//del
bool KV_OPT::DEL(const int& key){
    //use LRU to remove from cache, push slot index back onto freeSlots 
    std::lock_guard<std::mutex> lock(mtx);
    //delete from cache 
    //update list 
    auto it = cache.find(key); 
    if(it == cache.end()) return false; //nothing to delete/item not found

    int i = it->second; //idx 
    
    cache.erase(it);
    freeSlots.push_back(i); //add to freeslot 
    //no need to free arena, next SET will overwrite it with freeSlots
    unlink(i);//unlink
    return true; 

}

std::optional<float> KV_OPT::SIMILARITY(int key1, int key2){
    std::lock_guard<std::mutex> lock(mtx);
    //get both vectors 
    auto it1 = cache.find(key1);
    auto it2 = cache.find(key2);
    //if they both exist 
    if (it1 == cache.end() || it2 == cache.end()) return std::nullopt; //not sure what to return if invalid
    //dot product 
        //multipty each num and then add
        float dot_product = 0.0f, mag1 = 0.0f, mag2 = 0.0f; 
        const float* a = &arena[it1->second * dim];
        const float* b = &arena[it2->second * dim];

        for(int i = 0; i < dim; ++i){
            dot_product += a[i] * b[i];
            mag1 += a[i]*a[i]; 
            mag2 += b[i]*b[i]; 
        }
    //magnitude sqrt
    mag1 = std::sqrt(mag1);
    mag2 = std::sqrt(mag2);
    //divide -> dot product / (mag1 * mag2)
    if(mag1 == 0.0f || mag2 == 0.0f) return std::nullopt;

    return dot_product / (mag1 * mag2); 
    // 1 = similar
    // 0 = uncorrelated
    //-1 = opposite dir, (we are not doing negative? not sure)

}

std::vector<std::pair<int, float>> KV_OPT::TOPK(int query_key, int k){//get keys or vec? 
    std::lock_guard<std::mutex> lock(mtx);

    auto it = cache.find(query_key); 
    if(it == cache.end()) return {}; 

    const float* query_vec = &arena[it->second * dim];//get feat vec

    //precomput query_vec mag
    float mag_q = 0.0f;
    for(int i = 0; i < dim; ++i) mag_q += query_vec[i] * query_vec[i];
    mag_q = std::sqrt(mag_q); 
    if(mag_q == 0.0f) return {}; 

    std::vector<std::pair<int, float>> sim_scores; 
    sim_scores.reserve(cache.size()); 

    //calculate cosine simularity 
    for(const auto& [key, idx] : cache){
        if(key == query_key) continue; 

        const float * vec = &arena[idx * dim]; 
        float dot_product = 0.0f, mag_v = 0.0f;
         for(int i = 0; i < dim; ++i){
            dot_product += query_vec[i] * vec[i];
            mag_v += vec[i]*vec[i]; 
        }
        mag_v = std::sqrt(mag_v); 
        if(mag_v == 0.0f) continue; //dont want to divide by 0

        sim_scores.emplace_back(key, dot_product / (mag_q * mag_v));
    }
    
    int actual_k = std::min(k, static_cast<int>(sim_scores.size()));

    // partial_sort: only the top actual_k need to end up fully sorted
    std::partial_sort(
        sim_scores.begin(), sim_scores.begin() + actual_k, sim_scores.end(),
        [](const auto& x, const auto& y) { return x.second > y.second; } // descending similarity
    );

    sim_scores.resize(actual_k);
    return sim_scores;

}

//rule of 3
//destructor
KV_OPT::~KV_OPT() = default;


