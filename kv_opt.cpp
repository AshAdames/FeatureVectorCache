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

    //push index onto free_slots 
    free_slots.resize(capacity); 
    for(int i = 0; i < capacity; ++i){
        free_slots[i] = i; 
    }
}


//Set
bool KV_OPT::SET(const int& key, const std::vector<float>& vec){ 
    std::unique_lock<std::shared_mutex> lock(shd_mtx);
    if(vec.size() != dim) return false;
    //if free_slots is non empty, pop off an index and use it 
    auto it = cache.find(key);
    if(it != cache.end()){ //key exists just update it 
        int i = it->second;
        std::copy(vec.begin(), vec.end(), arena.begin() + i * dim);
        //update norm
        float norm = 0.0f;
        for (int j = 0; j < dim; ++j)
            norm += vec[j] * vec[j];

        pool[i].norm = std::sqrt(norm);
        // Refresh TTL
        pool[i].expiration = std::chrono::steady_clock::now() + TTL;
        
        unlink(i);
        push_front(i);
        return true;
    }

    int i;
    if(!free_slots.empty()){//get index 
        i = free_slots.back(); 
        free_slots.pop_back();
    }else{//no free slots, get the tail index, free that slot, use that index 
        i = tail;
        cache.erase(pool[i].key);
        unlink(i);
    }

    //create new key,vec,calcu norm (for SIMILIRITY + TOPK) push to front
    std::copy(vec.begin(), vec.end(), arena.begin() + i * dim);
    float norm = 0.0f;
    for (int j = 0; j < dim; ++j)
        norm += vec[j] * vec[j];
    pool[i].norm = std::sqrt(norm);
    pool[i].key = key;
    pool[i].expiration = std::chrono::steady_clock::now() + TTL; 
    cache[key] = i; 
    push_front(i);
    return true;
}
   
//Get
std::vector<float> KV_OPT::GET(const int& key){
    std::vector<float> result;
    int slot = -1;
    bool expired = false;

    // shared lock for read:  lookup, expiry check, copy vector out.
    // Multiple GETs can run this phase concurrently.
    {
        std::shared_lock<std::shared_mutex> lock(shd_mtx);
        auto it = cache.find(key);
        if (it == cache.end()) return {};

        slot = it->second;
        if (std::chrono::steady_clock::now() > pool[slot].expiration) {
            expired = true;
        } else {
            result.assign(arena.begin() + slot*dim, arena.begin() + slot*dim + dim);
        }
    }

    if (expired) {
        // exclusive lock to  remove the expired entry.
        // Re-check under the lock: another thread may have expired/evicted/overwritten key since phase 1
        std::unique_lock<std::shared_mutex> lock(shd_mtx);
        auto it = cache.find(key);
        if (it != cache.end() && it->second == slot &&
            std::chrono::steady_clock::now() > pool[slot].expiration) {
            cache.erase(it);
            unlink(slot);
            free_slots.push_back(slot);
        }
        return {};
    }

    //exclusive lock for LRU reorder only
    {
        std::unique_lock<std::shared_mutex> lock(shd_mtx);
        auto it = cache.find(key);
        if (it != cache.end() && it->second == slot) {
            unlink(slot);
            push_front(slot);
        }
    }

    return result;
}

//del
bool KV_OPT::DEL(const int& key){
    //use LRU to remove from cache, push slot index back onto free_slots 
    std::unique_lock<std::shared_mutex> lock(shd_mtx);
    //delete from cache 
    //update list 
    auto it = cache.find(key); 
    if(it == cache.end()) return false; //nothing to delete/item not found

    int i = it->second; //idx 
    
    cache.erase(it);
    free_slots.push_back(i); //add to freeslot 
    //no need to free arena, next SET will overwrite it with free_slots
    unlink(i);//unlink
    return true; 

}

// Requires -mavx2 -mfma. a and b must not overlap (asserted by __restrict).
static inline float dot_avx2(const float* __restrict a, const float* __restrict b, size_t dim) {
    __m256 acc = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        acc = _mm256_fmadd_ps(va, vb, acc);   // acc += va * vb, 8 lanes at once
    }

    // horizontal sum of the 8 floats in acc
    __m128 lo    = _mm256_castps256_ps128(acc);
    __m128 hi    = _mm256_extractf128_ps(acc, 1);
    __m128 sum4  = _mm_add_ps(lo, hi);
    __m128 sum2  = _mm_add_ps(sum4, _mm_movehl_ps(sum4, sum4));
    __m128 sum1  = _mm_add_ss(sum2, _mm_shuffle_ps(sum2, sum2, 1));
    float result = _mm_cvtss_f32(sum1);

    for (; i < dim; ++i) result += a[i] * b[i];   // scalar remainder if dim % 8 != 0
    return result;
}

// keep the scalar version too needed for the as a portable fallback if AVX2 isn't available at compile time.
static inline float dot_scalar(const float* a, const float* b, size_t dim) {
    float acc = 0.0f;
    for (size_t i = 0; i < dim; ++i) acc += a[i] * b[i];
    return acc;
}

static inline float dot_product(const float* a, const float* b, size_t dim) {
#ifdef __AVX2__
    return dot_avx2(a, b, dim);
#else
    return dot_scalar(a, b, dim);
#endif
}

std::optional<float> KV_OPT::SIMILARITY(int key1, int key2){
    std::shared_lock<std::shared_mutex> lock(shd_mtx);
    //get both vectors 
    auto it1 = cache.find(key1);
    auto it2 = cache.find(key2);
    //if they both exist 
    if (it1 == cache.end() || it2 == cache.end()) return std::nullopt; //not sure what to return if invalid
    //dot product 
    //multipty each num and then add
    const float* a = &arena[it1->second * dim];
    const float* b = &arena[it2->second * dim];

    float dot_product_val = dot_product(a, b, dim); 
    //divide -> dot product / (norm1* norm2)
    int i = it1->second, j = it2->second;
    if(pool[i].norm == 0.0f || pool[j].norm == 0.0f) return std::nullopt;

    return dot_product_val / (pool[i].norm * pool[j].norm); 
    // 1 = similar
    // 0 = uncorrelated
    //-1 = opposite dir, (we are not doing negative? not sure)

}

std::vector<std::pair<int, float>> KV_OPT::TOPK(int query_key, int k){//get keys or vec? 
    std::unique_lock<std::shared_mutex> lock(shd_mtx);

    auto it = cache.find(query_key); 
    if(it == cache.end()) return {}; 

    const float* query_vec = &arena[it->second * dim];//get feat vec

    //precomput query_vec mag
    float mag_q = pool[it->second].norm;
    if(mag_q == 0.0f) return {}; 

    std::vector<std::pair<int, float>> sim_scores; 
    sim_scores.reserve(cache.size()); 

    //calculate cosine simularity 
    for(const auto& [key, idx] : cache){
        if(key == query_key) continue; 

        const float * vec = &arena[idx * dim]; 
        float mag_v = pool[idx].norm;
        if(mag_v == 0.0f) continue; //dont want to divide by 0

        float dp = dot_product(query_vec, vec, dim);   // was the scalar loop
        sim_scores.emplace_back(key, dp / (mag_q * mag_v));
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


