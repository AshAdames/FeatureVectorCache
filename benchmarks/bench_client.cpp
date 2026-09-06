// bench_client.cpp
//
// Simple concurrent load generator for the KV TCP server.
// Not part of the server codebase — this is a standalone benchmarking tool.
//
// Usage:
//   bench_client <host> <port> <num_connections> <ops_per_connection> <workload>
//
//   workload: "get", "set", or "mixed" (90% GET / 10% SET)
//
// Example:
//   ./bench_client 127.0.0.1 8080 50 2000 mixed
//
// Each connection runs on its own std::thread, opens ONE blocking TCP
// connection, and issues ops_per_connection commands sequentially
// (send, wait for reply, repeat) — this measures per-connection
// request/response latency under N-way concurrency, not pipelining.

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <random>

using asio::ip::tcp;
using Clock = std::chrono::steady_clock;

struct Options {
    std::string host = "127.0.0.1";
    std::string port = "8080";
    int num_connections = 10;
    int ops_per_connection = 1000;
    std::string workload = "mixed"; // "get" | "set" | "mixed"
};

// Shared results, protected by a mutex (benchmark client itself doesn't
// need to be lock-free — it's not the thing being measured).
std::mutex g_results_mtx;
std::vector<double> g_latencies_us; // one entry per completed op, in microseconds
std::atomic<long> g_errors{0};

// Warm-up ops run before timing starts, per connection, to avoid
// counting TCP handshake / connection-setup noise in steady-state numbers.
constexpr int WARMUP_OPS = 50;

std::string build_command(std::mt19937& rng, const std::string& workload, int conn_id, int op_idx) {
    // Spread keys across a fixed small keyspace per connection so SET/GET
    // have something to hit — real cache workloads aren't all-unique keys.
    std::string key = "key" + std::to_string(conn_id) + "_" + std::to_string(op_idx % 100);

    if (workload == "get") {
        return "GET " + key + "\n";
    } else if (workload == "set") {
        return "SET " + key + " val" + std::to_string(op_idx) + "\n";
    } else { // mixed: 90% GET / 10% SET
        std::uniform_int_distribution<int> dist(0, 99);
        if (dist(rng) < 10) {
            return "SET " + key + " val" + std::to_string(op_idx) + "\n";
        } else {
            return "GET " + key + "\n";
        }
    }
}

void run_connection(const Options& opts, int conn_id) {
    try {
        asio::io_context io;
        tcp::resolver resolver(io);
        auto endpoints = resolver.resolve(opts.host, opts.port);

        tcp::socket socket(io);
        asio::connect(socket, endpoints);

        std::mt19937 rng(conn_id * 7919u + 12345u);
        asio::streambuf read_buf;

        std::vector<double> local_latencies;
        local_latencies.reserve(opts.ops_per_connection);

        // Pre-set a couple of keys for this connection so GETs have
        // something real to find during the timed phase (avoids every
        // GET returning "(nil)" which is a much cheaper code path).
        {
            std::string seed_cmd = "SET key" + std::to_string(conn_id) + "_0 seedval\n";
            asio::write(socket, asio::buffer(seed_cmd));
            asio::read_until(socket, read_buf, '\n');
            read_buf.consume(read_buf.size());
        }

        int total_ops = WARMUP_OPS + opts.ops_per_connection;
        for (int i = 0; i < total_ops; ++i) {
            std::string cmd = build_command(rng, opts.workload, conn_id, i);
            bool timed = (i >= WARMUP_OPS);

            Clock::time_point t0;
            if (timed) t0 = Clock::now();

            std::error_code ec;
            asio::write(socket, asio::buffer(cmd), ec);
            if (!ec) {
                asio::read_until(socket, read_buf, '\n', ec);
            }
            if (ec) {
                g_errors.fetch_add(1, std::memory_order_relaxed);
                break; // connection likely dropped — stop this worker
            }
            read_buf.consume(read_buf.size()); // discard response bytes, already "received"

            if (timed) {
                auto t1 = Clock::now();
                double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                local_latencies.push_back(us);
            }
        }

        std::lock_guard<std::mutex> lock(g_results_mtx);
        g_latencies_us.insert(g_latencies_us.end(), local_latencies.begin(), local_latencies.end());

    } catch (std::exception& e) {
        std::cerr << "[conn " << conn_id << "] exception: " << e.what() << "\n";
        g_errors.fetch_add(1, std::memory_order_relaxed);
    }
}

double percentile(std::vector<double>& sorted_data, double p) {
    if (sorted_data.empty()) return 0.0;
    size_t idx = static_cast<size_t>(p * (sorted_data.size() - 1));
    return sorted_data[idx];
}

int main(int argc, char** argv) {
    Options opts;
    if (argc >= 2) opts.host = argv[1];
    if (argc >= 3) opts.port = argv[2];
    if (argc >= 4) opts.num_connections = std::atoi(argv[3]);
    if (argc >= 5) opts.ops_per_connection = std::atoi(argv[4]);
    if (argc >= 6) opts.workload = argv[5];

    std::cout << "=== KV Server Benchmark ===\n";
    std::cout << "host:port         = " << opts.host << ":" << opts.port << "\n";
    std::cout << "connections       = " << opts.num_connections << "\n";
    std::cout << "ops/connection    = " << opts.ops_per_connection << " (+" << WARMUP_OPS << " warmup, untimed)\n";
    std::cout << "workload          = " << opts.workload << "\n";
    std::cout << "----------------------------\n";

    g_latencies_us.reserve(static_cast<size_t>(opts.num_connections) * opts.ops_per_connection);

    auto bench_start = Clock::now();

    std::vector<std::thread> threads;
    threads.reserve(opts.num_connections);
    for (int i = 0; i < opts.num_connections; ++i) {
        threads.emplace_back(run_connection, std::cref(opts), i);
    }
    for (auto& t : threads) t.join();

    auto bench_end = Clock::now();
    double total_seconds = std::chrono::duration<double>(bench_end - bench_start).count();

    std::vector<double> sorted_latencies = g_latencies_us; // copy to sort
    std::sort(sorted_latencies.begin(), sorted_latencies.end());

    size_t total_ops = sorted_latencies.size();
    double throughput = total_ops / total_seconds;

    std::cout << "\n=== Results ===\n";
    std::cout << "total ops completed : " << total_ops << "\n";
    std::cout << "errors               : " << g_errors.load() << "\n";
    std::cout << "wall time (s)        : " << total_seconds << "\n";
    std::cout << "throughput (ops/sec) : " << throughput << "\n";
    if (!sorted_latencies.empty()) {
        std::printf("p50 latency (us)     : %.2f\n", percentile(sorted_latencies, 0.50));
        std::printf("p95 latency (us)     : %.2f\n", percentile(sorted_latencies, 0.95));
        std::printf("p99 latency (us)     : %.2f\n", percentile(sorted_latencies, 0.99));
        std::printf("p999 latency (us)    : %.2f\n", percentile(sorted_latencies, 0.999));
        std::printf("max latency (us)     : %.2f\n", sorted_latencies.back());
    }

    return 0;
}