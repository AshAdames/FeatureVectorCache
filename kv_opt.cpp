#include "KV_OPT_opt.h"

/* TODO TODAY
- intrinsct list - fix Node 
- change val from string to fixed float 



*/

/* General TODO 

- fix protocol for vector of floats
- implement SIMILARIRTY key1 key2 scalar cosine 
- correctness tests 
- implement TOPK
- shard by key 
re benchmark
*/

//helper functions for adding/removing to linked list
//-1 means empty
void KV_OPT::unlink(int i){
    //removing from linked list, remove and relink neighbots (prev, next)
    int prev = pool[i].prev;
    int next = pool[i].next;
    int node = pool[i];
    // A -> i -> C , remove B
    if(prev != -1){
        pool[node.prev].next = node.next; //unlink it 
    }
    else{  //found head, no prev
        head = n
    }; //head = -1
    
    if(node.next != -1) {
        pool[node.next].prev = node.prev; //unlink
    } 
    else{//found tail, no next
        tail = node.prev; 
    }

    pool[i].prev = pool[i].next = -1; //free index 

}



void KV_OPT::push_front(int i){
    pool[i].prev = -1; 
    pool[i].next = head; 
    if(head != 01) pool[head].prev = i; 
    head = i; 
    if(tail == -1) tail = i; //empty list 
}


//constructor 
KV_OPT::KV_OPT(int cap, int ttl, float dim){
    capacity = cap;
    dim = dim;
    //default ttl for entire KV_OPT, in secs
    TTL = std::chrono::seconds(ttl);

    //push index onto freeSlots 
    freeSlots.reserve(n); 
    for(int i = 0; i < n; ++i){
        freeSlots[i] = i; 
    }
}


//where does cache fit in?
//still need to do fast lookup of feature vectors
//what about updating head and tail?
//Set
bool KV_OPT::SET(const int& key, std::vector<float>& vec){ 
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
    pool[i].expiration = chrono::stead_clock::now() + TTL; 
    cache[key] = i; 
    push_front(i);
    return true;
}
   
//Get
string KV_OPT::GET(const int& key){
    //use hashmap to determine if it exists 
    std::lock_guard<std::mutex> lock(mtx);
    auto it = cache.find(key)
    if(it == cache.end()) return {}

    int i = it->second; 
    if(chrono::stead_clock::now() > pool[i].expiration){
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

float KV_OPT::SIMILARITY(int key1, int key2){

}

//rule of 3
//destructor
KV_OPT::~KV_OPT() {
    for (auto* n : valList) delete n;
}



