# 3. Phase 1 — vector specialization

* Swap Node::val from std::string to a fixed-length float32[N], with length validation on SET
* Extend the protocol to send/receive a vector of floats (length-prefixed or comma-separated to start)
* Implement SIMILARITY key1 key2 (scalar cosine similarity or dot product)
* Implement TOPK query_vec k (brute-force scalar scan)
* Correctness tests for these: hand-computed similarity on known vectors, zero-vector/identical-vector/orthogonal edge cases

# 4. Phase 2 — concurrency optimization

* Profile the global lock under load with the vector workload
* Shard by key (or your chosen alternative), re-implement
*  Re-benchmark against the Phase 0 baseline — this is optimization story #1

# 5. Phase 4 — the headline benchmark

* Compare in-process SIMILARITY/TOPK against round-tripping vectors through a generic KV GET + client-side numpy computation — this produces your "Nx faster" resume stat

# 6. Stretch — SIMD

* AVX2 (or equivalent) rewrite of the scalar similarity loop
* Correctness check against scalar output within float tolerance
* Scalar vs. SIMD benchmark — optimization story #2