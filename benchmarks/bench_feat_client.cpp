
// bench_feat_client.cpp
//
// Concurrent benchmark client for the KV_OPT binary TCP protocol.
//
// Usage:
//   bench_feat_client <host> <port> <num_connections>
//                     <ops_per_connection> <workload>
//
// Workloads:
//   get          100% GET
//   set          100% SET
//   mixed        90% GET / 10% SET
//   similarity   100% SIMILARITY
//   topk         100% TOPK
//
// Example:
//   ./bench_feat_client 127.0.0.1 8080 50 1000 mixed
//
// Protocol:
//
// Request:
//   [msg_header]
//   [float payload...]       only for SET
//
// Response from current server:
//   [msg_header]
//   [float payload...]       only for GET
//
// cmd:
//   0 = GET
//   1 = SET
//   2 = DEL
//   3 = SIMILARITY
//   4 = TOPK
//

#include <asio.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

using asio::ip::tcp;
using Clock = std::chrono::steady_clock;

constexpr int VECTOR_DIM = 4;
constexpr int WARMUP_OPS = 50;
constexpr int SEED_KEYS = 100;
constexpr int TOPK_K = 10;

// ------------------------------------------------------------
// Protocol structs
// ------------------------------------------------------------

struct msg_header {
    int cmd;
    int key;
    int key2 = -1;
    int k;
    int payload_len;
};

// ------------------------------------------------------------
// Options
// ------------------------------------------------------------

struct Options {
    std::string host = "127.0.0.1";
    std::string port = "8080";

    int num_connections = 10;
    int ops_per_connection = 1000;

    std::string workload = "mixed";
};

// ------------------------------------------------------------
// Shared benchmark results
// ------------------------------------------------------------

std::mutex g_results_mtx;
std::vector<double> g_latencies_us;
std::atomic<long> g_errors{0};

// ------------------------------------------------------------
// Build deterministic vector
// ------------------------------------------------------------

std::vector<float> make_vector(int key, int op_idx) {
    return {
        static_cast<float>((key + op_idx) % 10 + 1),
        static_cast<float>((key + op_idx * 2) % 10 + 1),
        static_cast<float>((key + op_idx * 3) % 10 + 1),
        static_cast<float>((key + op_idx * 4) % 10 + 1)
    };
}

// ------------------------------------------------------------
// Build GET request
// ------------------------------------------------------------

msg_header build_get_header(int key) {
    msg_header hdr{};

    hdr.cmd = 0;
    hdr.key = key;
    hdr.key2 = -1;
    hdr.k = 0;
    hdr.payload_len = 0;

    return hdr;
}

// ------------------------------------------------------------
// Build SET request
// ------------------------------------------------------------

msg_header build_set_header(int key) {
    msg_header hdr{};

    hdr.cmd = 1;
    hdr.key = key;
    hdr.key2 = -1;
    hdr.k = 0;
    hdr.payload_len = VECTOR_DIM;

    return hdr;
}

// ------------------------------------------------------------
// Build SIMILARITY request
// ------------------------------------------------------------

msg_header build_similarity_header(int key1, int key2) {
    msg_header hdr{};

    hdr.cmd = 3;
    hdr.key = key1;
    hdr.key2 = key2;
    hdr.k = 0;
    hdr.payload_len = 0;

    return hdr;
}

// ------------------------------------------------------------
// Build TOPK request
// ------------------------------------------------------------

msg_header build_topk_header(int key, int k) {
    msg_header hdr{};

    hdr.cmd = 4;
    hdr.key = key;
    hdr.key2 = -1;
    hdr.k = k;
    hdr.payload_len = 0;

    return hdr;
}

// ------------------------------------------------------------
// Send complete request
// ------------------------------------------------------------

void send_get(
    tcp::socket& socket,
    int key
) {
    msg_header hdr = build_get_header(key);

    asio::write(
        socket,
        asio::buffer(&hdr, sizeof(hdr))
    );
}

// ------------------------------------------------------------

void send_set(
    tcp::socket& socket,
    int key,
    int op_idx
) {
    msg_header hdr = build_set_header(key);

    std::vector<float> vec =
        make_vector(key, op_idx);

    std::vector<asio::const_buffer> buffers = {
        asio::buffer(&hdr, sizeof(hdr)),
        asio::buffer(vec.data(), vec.size() * sizeof(float))
    };

    asio::write(socket, buffers);
}

// ------------------------------------------------------------

void send_similarity(
    tcp::socket& socket,
    int key1,
    int key2
) {
    msg_header hdr =
        build_similarity_header(key1, key2);

    asio::write(
        socket,
        asio::buffer(&hdr, sizeof(hdr))
    );
}

// ------------------------------------------------------------

void send_topk(
    tcp::socket& socket,
    int key,
    int k
) {
    msg_header hdr =
        build_topk_header(key, k);

    asio::write(
        socket,
        asio::buffer(&hdr, sizeof(hdr))
    );
}

// ------------------------------------------------------------
// Read response from current server
//
// Current server sends:
//
//   GET:
//       [header][vector<float>]
//
//   SET/DEL/SIMILARITY/TOPK:
//       [header]
//
// ------------------------------------------------------------

void read_response(
    tcp::socket& socket,
    int request_cmd
) {
    msg_header response_hdr{};

    asio::read(
        socket,
        asio::buffer(&response_hdr, sizeof(response_hdr))
    );

    // Current server's GET response contains a vector.
    if (request_cmd == 0 &&
        response_hdr.payload_len > 0) {

        std::vector<float> response_vec(
            response_hdr.payload_len
        );

        asio::read(
            socket,
            asio::buffer(
                response_vec.data(),
                response_vec.size() * sizeof(float)
            )
        );
    }

    // The current server does not send:
    //
    //   SIMILARITY result
    //   TOPK results
    //   status
    //
    // so there is nothing else to consume for those.
}

// ------------------------------------------------------------
// Run one connection
// ------------------------------------------------------------

void run_connection(
    const Options& opts,
    int conn_id
) {
    try {
        asio::io_context io;

        tcp::resolver resolver(io);

        auto endpoints =
            resolver.resolve(
                opts.host,
                opts.port
            );

        tcp::socket socket(io);

        asio::connect(
            socket,
            endpoints
        );

        std::mt19937 rng(
            static_cast<unsigned int>(
                conn_id * 7919u + 12345u
            )
        );

        std::vector<double> local_latencies;

        local_latencies.reserve(
            opts.ops_per_connection
        );

        // ----------------------------------------------------
        // Seed keys
        //
        // conn 0 -> 0-99
        // conn 1 -> 100-199
        // conn 2 -> 200-299
        // ----------------------------------------------------

        for (int i = 0; i < SEED_KEYS; ++i) {

            int key =
                conn_id * SEED_KEYS + i;

            send_set(
                socket,
                key,
                i
            );

            // SET response = header only
            read_response(
                socket,
                1
            );
        }

        // ----------------------------------------------------
        // Warmup + timed operations
        // ----------------------------------------------------

        int total_ops =
            WARMUP_OPS +
            opts.ops_per_connection;

        for (int i = 0;
             i < total_ops;
             ++i) {

            int base_key =
                conn_id * SEED_KEYS;

            int key =
                base_key + (i % SEED_KEYS);

            bool timed =
                i >= WARMUP_OPS;

            Clock::time_point t0;

            if (timed) {
                t0 = Clock::now();
            }

            // ------------------------------------------------
            // Determine workload
            // ------------------------------------------------

            int request_cmd = 0;

            if (opts.workload == "get") {

                request_cmd = 0;

                send_get(
                    socket,
                    key
                );

            } else if (opts.workload == "set") {

                request_cmd = 1;

                send_set(
                    socket,
                    key,
                    i
                );

            } else if (opts.workload == "similarity") {

                request_cmd = 3;

                int key2 =
                    base_key +
                    ((i + 1) % SEED_KEYS);

                send_similarity(
                    socket,
                    key,
                    key2
                );

            } else if (opts.workload == "topk") {

                request_cmd = 4;

                send_topk(
                    socket,
                    key,
                    TOPK_K
                );

            } else {

                // mixed = 90% GET / 10% SET

                std::uniform_int_distribution<int> dist(
                    0,
                    99
                );

                if (dist(rng) < 10) {

                    request_cmd = 1;

                    send_set(
                        socket,
                        key,
                        i
                    );

                } else {

                    request_cmd = 0;

                    send_get(
                        socket,
                        key
                    );
                }
            }

            // ------------------------------------------------
            // Read response
            // ------------------------------------------------

            read_response(
                socket,
                request_cmd
            );

            if (timed) {

                auto t1 = Clock::now();

                double us =
                    std::chrono::duration<double, std::micro>(
                        t1 - t0
                    ).count();

                local_latencies.push_back(us);
            }
        }

        // ----------------------------------------------------
        // Merge results
        // ----------------------------------------------------

        {
            std::lock_guard<std::mutex> lock(
                g_results_mtx
            );

            g_latencies_us.insert(
                g_latencies_us.end(),
                local_latencies.begin(),
                local_latencies.end()
            );
        }

    } catch (const std::exception& e) {

        std::cerr
            << "[conn "
            << conn_id
            << "] exception: "
            << e.what()
            << "\n";

        g_errors.fetch_add(
            1,
            std::memory_order_relaxed
        );
    }
}

// ------------------------------------------------------------
// Percentile
// ------------------------------------------------------------

double percentile(
    const std::vector<double>& sorted_data,
    double p
) {
    if (sorted_data.empty()) {
        return 0.0;
    }

    size_t idx =
        static_cast<size_t>(
            p * (sorted_data.size() - 1)
        );

    return sorted_data[idx];
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main(int argc, char** argv) {

    Options opts;

    if (argc >= 2)
        opts.host = argv[1];

    if (argc >= 3)
        opts.port = argv[2];

    if (argc >= 4)
        opts.num_connections =
            std::atoi(argv[3]);

    if (argc >= 5)
        opts.ops_per_connection =
            std::atoi(argv[4]);

    if (argc >= 6)
        opts.workload = argv[5];

    // --------------------------------------------------------
    // Validate
    // --------------------------------------------------------

    if (
        opts.num_connections < 1 ||
        opts.ops_per_connection < 1
    ) {
        std::cerr
            << "connections and ops must be >= 1\n";

        return 1;
    }

    if (
        opts.workload != "get" &&
        opts.workload != "set" &&
        opts.workload != "mixed" &&
        opts.workload != "similarity" &&
        opts.workload != "topk"
    ) {
        std::cerr
            << "Invalid workload: "
            << opts.workload
            << "\n";

        std::cerr
            << "Valid workloads: "
            << "get, set, mixed, similarity, topk\n";

        return 1;
    }

    // --------------------------------------------------------
    // Header
    // --------------------------------------------------------

    std::cout
        << "=== KV_OPT Server Benchmark: Capcity:10000 ===\n";

    std::cout
        << "host:port         = "
        << opts.host
        << ":"
        << opts.port
        << "\n";

    std::cout
        << "connections       = "
        << opts.num_connections
        << "\n";

    std::cout
        << "ops/connection    = "
        << opts.ops_per_connection
        << " (+"
        << WARMUP_OPS
        << " warmup, untimed)\n";

    std::cout
        << "workload          = "
        << opts.workload
        << "\n";

    std::cout
        << "----------------------------\n";

    // --------------------------------------------------------
    // Reserve
    // --------------------------------------------------------

    g_latencies_us.reserve(
        static_cast<size_t>(
            opts.num_connections
        ) *
        opts.ops_per_connection
    );

    // --------------------------------------------------------
    // Start benchmark
    // --------------------------------------------------------

    auto bench_start =
        Clock::now();

    std::vector<std::thread> threads;

    threads.reserve(
        opts.num_connections
    );

    for (int i = 0;
         i < opts.num_connections;
         ++i) {

        threads.emplace_back(
            run_connection,
            std::cref(opts),
            i
        );
    }

    for (auto& t : threads) {
        t.join();
    }

    // --------------------------------------------------------
    // End benchmark
    // --------------------------------------------------------

    auto bench_end =
        Clock::now();

    double total_seconds =
        std::chrono::duration<double>(
            bench_end - bench_start
        ).count();

    // --------------------------------------------------------
    // Sort
    // --------------------------------------------------------

    std::vector<double> sorted_latencies =
        g_latencies_us;

    std::sort(
        sorted_latencies.begin(),
        sorted_latencies.end()
    );

    // --------------------------------------------------------
    // Results
    // --------------------------------------------------------

    size_t total_ops =
        sorted_latencies.size();

    double throughput =
        total_ops / total_seconds;

    std::cout
        << "\n=== Results ===\n";

    std::cout
        << "total ops completed : "
        << total_ops
        << "\n";

    std::cout
        << "errors               : "
        << g_errors.load()
        << "\n";

    std::cout
        << "wall time (s)        : "
        << total_seconds
        << "\n";

    std::cout
        << "throughput (ops/sec) : "
        << throughput
        << "\n";

    if (!sorted_latencies.empty()) {

        std::printf(
            "p50 latency (us)     : %.2f\n",
            percentile(
                sorted_latencies,
                0.50
            )
        );

        std::printf(
            "p95 latency (us)     : %.2f\n",
            percentile(
                sorted_latencies,
                0.95
            )
        );

        std::printf(
            "p99 latency (us)     : %.2f\n",
            percentile(
                sorted_latencies,
                0.99
            )
        );

        std::printf(
            "p999 latency (us)    : %.2f\n",
            percentile(
                sorted_latencies,
                0.999
            )
        );

        std::printf(
            "max latency (us)     : %.2f\n",
            sorted_latencies.back()
        );
    }

    return 0;
}