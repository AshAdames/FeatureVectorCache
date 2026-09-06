
- basic KV store DONE
- ASIO server DONE


Next Steps 


threading model:
    thread pool + shared io_context
        - multiple threads all call run(). multiple sessions run on different threads, but share teh KV 

adding lock the KV store itself 
    mutex vs shared_mutex 




- Switch io_context::run() to run on a thread pool (small code change in main.cpp).
- Add a shared_mutex (or plain mutex first, upgrade later) to KV, locking around cache/valList access in GET/SET/DEL.
- Add ThreadSanitizer to your build as a debug config, and write a couple of concurrent stress tests.
