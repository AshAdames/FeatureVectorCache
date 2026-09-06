# Design of the benchmark client:

Uses ASIO (you already depend on it), spawns N concurrent connections (each on its own thread for simplicity — this is a load generator, not the thing being optimized).
Each connection loops: build a command string, record steady_clock::now(), blocking write + read_until('\n'), record time again, push the delta into a thread-local latency vector.
After all threads finish, merge all latency vectors, sort, report count, total time, ops/sec, p50/p99/p999.
Configurable: host, port, num connections, ops per connection, workload (GET/SET/mixed).


# terminal 1
./kv_server

# terminal 2 — build and run benchmark
cmake --build build -j$(nproc)
./build/bench_client 127.0.0.1 8080 50 1000 mixed | tee results_baseline_n50.txt


# run concurrecny sweep:

for n in 1 10 50 100; do
    echo "--- $n connections ---"
    ./build/bench_client 127.0.0.1 8080 $n 1000 mixed
done

What it measures:

Real per-request round-trip latency (write command → read response) over an actual blocking socket, one connection per thread — mirrors real client behavior, not pipelined/batched.
Warm-up ops (50, untimed) exclude TCP handshake/connection-setup noise from your steady-state numbers, per what we discussed.
p50/p95/p99/p999 + throughput, so you can see both the "typical" and "tail" latency — tail latency is what'll expose lock contention as you add more connections, even if average throughput looks fine.

Run this against your current single-threaded server first to get your baseline table (ops/sec + p50/p99 at N=1,10,50,100), then rerun unchanged once you add the thread pool — same script, no changes needed, since it just measures the server as a black box over TCP.

