#!/bin/bash

CONNECTIONS=(1 5 25 50 100)
WORKLOADS=("mixed" "get" "topk" "set")

for workload in "${WORKLOADS[@]}"; do
    for connections in "${CONNECTIONS[@]}"; do

        echo "========================================"
        echo "Workload: $workload | Connections: $connections"
        echo "========================================"

        ./bench_client \
            127.0.0.1 \
            8080 \
            "$connections" \
            1000 \
            "$workload" \
            | tee "results_shared_cached-norm_SIMD_${workload}_n${connections}.txt"

        echo ""
    done
done