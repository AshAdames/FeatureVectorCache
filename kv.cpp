#include "kv.h"

/* TODO TODAY
multithreading 


*/

/* General TODO 

KV Functions
- GET , SET, DEL, EXPIRE 
- LRU eviction 

Network + Protocol 
- parsing protocol
- tcp server 
- single connection 

Concurrency 
- initially with lock 
- multithreaded 

Optimizations 
- fragmentation?
    - remove DLL
- cache lines 
- sharding 
- minor upgrade over memcache (ML specific?)

Benchmarking
- lock vs sharding

//make value agnostic ?
*/

//Mutex where ? 
// 

//constructor 
KV::KV(int cap, int ttl){
    capacity = cap;
    //default ttl for entire KV, in secs
    TTL = std::chrono::seconds(ttl);
}

//Set
bool KV::SET(const string& key, const string& val){ 
    std::lock_guard<std::mutex> lock(mtx);
    if(key.empty()) return false; //cannot set empty key

    auto it = cache.find(key);// it-> first = key, it->second = val (node iters)
    if(it == cache.end()){//not in cache, lets insert 
        if( cache.size() >= capacity){//evict at cap
           //at limit remove from back
            Node * n = valList.back(); //get node from back
            valList.pop_back(); // remove from list
            cache.erase(n->key); //remove from cache
            delete n; 
        }
     
    }else{//key already exists 
        Node * n = *it->second; 
        //check expiration
        if(std::chrono::steady_clock::now() > n->expiration){
            //delete 
            //delete from map
            valList.erase(it->second); 
            cache.erase(it);
            delete n;
        }else{
            //update existing node
             n->val = val;
            //erase from list 
            n->expiration = chrono::steady_clock::now() + TTL;
            valList.erase(it->second);
            valList.push_front(n);
            it->second = valList.begin();
            return true;
        }
    }
        //inserts a new if expired and if not in cache +cap allows
        Node * n = new Node(key, val, TTL);
        valList.push_front(n);  //automatically at front 
        cache[key] = valList.begin(); //adding iter to cache
        return true; 
}
   
//Get
string KV::GET(const string& key){
    std::lock_guard<std::mutex> lock(mtx); 
    auto it = cache.find(key);
    if(it != cache.end()){//found 
        //lazy expire check
        //check if time > than TLL then delete
        if(chrono::steady_clock::now() < (*it->second)->expiration){// not expired
            //splice is O(1)
            valList.splice(valList.begin(), valList, it->second);
            return (*it->second)->val;
        }else{
            //expired 
            //delete it 
            Node* tmp = *it->second;
            valList.erase(it->second); //passing iterator 
            cache.erase(it); 
            delete tmp; //actual Node * 
            return ""; 
        }

    }
    return ""; //no key found 

}

//del
bool KV::DEL(const string& key){
    std::lock_guard<std::mutex> lock(mtx);
    auto it = cache.find(key); 
        //it : first = key, second = list iter
    if(it != cache.end()){//exists
        Node * n = *it->second; //ptr to node
        //erase from list, then map
        valList.erase(it->second);
        cache.erase(it); 
        delete n; 
        //erase for list
        return true; 

    }else{//nothing to erase
        return false; 
    }

}

//rule of 3
//destructor
KV::~KV() {
    for (auto* n : valList) delete n;
}



