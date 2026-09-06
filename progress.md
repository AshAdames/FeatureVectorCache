## start-to-end architecture

**1. Storage layer (`kv.h`/`kv.cpp`)  COMPLETED **
- Hash map (`cache`) + doubly linked list (`valList`) = classic LRU cache.
- `cache[key]` → iterator into `valList`, so lookups are O(1) and "move to front" (recency) is O(1) via `splice`.
- `SET`/`GET`/`DEL` implemented, with per-key TTL-based expiration checked lazily (on access, not via a background sweep).

**2. Session layer (`session.hpp`/`session.cpp`) 
- One `Session` object per connected client, kept alive via `shared_from_this()` during async ops (so it doesn't get destroyed mid-I/O).
- Holds a `KV&` reference — **not** its own copy — so all sessions share the *same* KV store instance.
- Flow per client: `start()` → `do_read()` waits for a line ending in `\n` → parses it in `handle_command()` → calls into the shared `KV` → `do_write(response)` sends the reply → on write completion, loops back to `do_read()` → repeats until the client disconnects or errors.

**3. Server layer (`server.hpp`, being fixed) — in progress**
- Owns the `tcp::acceptor`, bound to port 8080.
- `do_accept()` recursively re-arms itself after each accept, so the server handles unlimited *sequential and concurrent* clients on a single thread — this is the "N clients, 1 event loop" pattern.
- Owns/receives a reference to the single shared `KV` store, passed down into each new `Session`.

**4. `main()` — the glue**
- Creates one `io_context`, one `KV store`, one `Server`.
- Calls `io.run()` exactly once — this is the actual event loop; it blocks and processes all pending async work (accepts, reads, writes) until told to stop or there's nothing left.

**End-to-end request flow right now:**
```
client connects (nc/telnet)
  → Server::do_accept() fires → creates Session, calls start()
  → Session::do_read() waits for a line
  → client types "SET foo bar\n"
  → do_read()'s handler fires → handle_command() parses it → key_store_.SET("foo","bar")
  → do_write("OK\n") sends response
  → on write complete → do_read() again, waiting for the next command
```


Run the server:

bash
./kv_server

Run unit tests:

bash
ctest
# or directly:
./tests/kv_tests

Manual network test (separate terminal, while kv_server is running):

bash
nc localhost 8080

Then type:

SET foo bar
GET foo
GET missing
DEL foo
GET foo

Expect:

OK
bar
(nil)
OK
(nil)

Test shared state across clients — open a second terminal:

bash
nc localhost 8080


