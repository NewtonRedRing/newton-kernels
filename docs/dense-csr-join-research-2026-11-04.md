# Experimental competitive-kernel frontier: dense CSR join

**Research date:** 2026-11-04. **Not a production performance claim.**

## Objective

Win on equivalent workloads against strong external libraries by improving **latency, throughput, allocation traffic, working-set size, cache misses, tail latency and power** where measurable. A fast path must preserve semantics, portability and memory safety. No single algorithm can dominate every input and every metric: select algorithms based on validated workload characteristics and report tradeoffs.

The production **22-case plus 6-case regression guard and its 3% limit are unchanged**. Do not drop cases or reduce confidence to pass optimizations.

## Prototype: bounded integer-key CSR join

New opt-in `include/newton/analytics/dense_join.hpp` adds `dense_join_into`. This method is specialized for a **known, contiguous key domain**, returning every duplicate-preserving join pair (original row indices) without allocating or hashing inside the call.

1. Validate the right keys and scratch capacity before modifying scratch.
2. Count right rows in each integer-key bucket.
3. Prefix-sum buckets into offsets and reverse-scatter original row indices into a contiguous, stable CSR-style posting array, using only the caller's `heads` and `next` scratch.
4. For every left key, traverse the corresponding contiguous posting span; keys outside the domain simply do not match.

Work is `O(domain_width + left_rows + right_rows + matching_pairs)`; scratch is `domain_width + right_rows` machine words. This is **not suited to sparse or large key domains**, where the existing general hash join remains the alternative. The implementation validates extreme `uint64_t` domain boundaries without addition overflow. Scratch must not overlap input spans.

### Failed initial approach and what changed

An initial direct-address join with linked per-key row chains performed **worse** than an optimized `unordered_map<key, vector<row_index>>` for highly duplicated keys, because the output visited scattered memory. CSR contiguous posting groups eliminated the random pointer chasing and changed the results. Merely deleting hash operations was not enough: **memory locality was the decisive improvement**.

### Local exploratory screen, not CI validation

Command: build `newton_dense_join_bench` under `NEWTON_BUILD_BENCHMARKS=ON`. The single-process fixture uses 32,000 left and 16,000 right `uint64_t` rows, four domain widths, a fixed seed, alternating execution order over 20 rounds, an optimized STL grouped-vector hash join that reserves buckets, and matching pair-count/checksum validation. Fixture generation is excluded; both paths time their index construction and all pair visits. The direct path uses preallocated reusable scratch; this policy differs from the STL reference's fresh per-call map and must be explicitly accounted for in future fair end-to-end comparisons.

| Domain | Matching pairs | CSR median (ms) | STL median (ms) | STL/CSR |
|---:|---:|---:|---:|---:|
| 256 | 2,000,950 | 3.86917 | 5.83946 | **1.51x** |
| 4,096 | 124,568 | 0.80356 | 3.23743 | **4.03x** |
| 65,536 | 7,817 | 0.43108 | 4.12559 | **9.57x** |
| 262,144 | 1,955 | 0.45561 | 2.86948 | **6.30x** |

One local container session; **no stability intervals, independent runner, memory-allocation measurement, p99 or competitor-wide validation**. The 262,144-key case uses ~2.1 MiB of CSR index scratch on a 64-bit machine, so a faster result does **not** automatically mean better memory efficiency. These numbers should not appear as shipped performance claims or on the primary public dashboard. Different hosts and cache states may reverse results.

### Correctness verification

`tests/unit/test_dense_join.cpp` compares the complete pair multiset with a quadratic oracle for **1,400 randomized trials**, plus empty, duplicates, keys outside the domain, `UINT64_MAX` boundaries and invalid scratch/domain conditions. Locally passed GCC and Clang C++20 Release builds, with checks enabled even under `NDEBUG`, plus GCC AddressSanitizer/UndefinedBehaviorSanitizer. These tests are now part of the project test matrix; **CI results must still be verified on the PR's final commit**.

## Competitive algorithm research program

| Priority | Component | Algorithmic research direction |
|---|---|---|
| P0 | General join | Compare specialized dense CSR, flat hash, SwissTable-style metadata, cache-partitioned radix join, sorted merge and duplicate-aware row grouping. Detect skew and domain width; control scratch budgets. PR #70 addresses generic join separately. |
| P0 | Grouping | Dense direct-address counters for bounded keys; SIMD metadata probing for large key sets; bounded preallocation. PR #71 covers generic pre-reservation. |
| P0 | CPU dispatch | Process-lifetime capabilities cache; select ISA strategy outside hot loops. PR #69 addresses this separately. |
| P1 | Inverted indexing | Adaptive rarest-first, galloping, SIMD postings intersection, Roaring-style array/bitmap/run containers; measure build-time and memory tradeoffs. |
| P1 | CSV & numeric | Quote-mask structural scan, zero-copy filtered fields, fused numeric filter/group, exact decimal conversion with contract-preserving fallbacks. |
| P1 | Graph | Workload-aware binary/radix/bucket priority queues, compact CSR, reused workspace, edge-weight-bounded specialized algorithms. |
| P2 | Parallel | NUMA-aware partitioning, caller-owned per-thread scratch, work-weighted scheduling, bounded threads. |
| P2 | Hardware | Portable SIMD variants (AVX2, AVX-512, ARM NEON), algorithmic instruction/cycle profiling, representative PGO/LTO. |

**Validation contract:** benchmark the strongest credible semantically equivalent reference, preserve output checksums and error behavior, compare measured build+query and steady-state modes separately, publish all losses, and add unseen skew/cardinality/size distributions before promoting a variant. Compare peak RSS, allocations, p95/p99 and end-to-end throughput as well as median microseconds. Only merge performance work after all existing CI, sanitizers, fuzz tests and both performance guards pass.

## Independent research inspirations

- [Abseil Swiss Tables](https://abseil.io/about/design/swisstables) (SIMD-friendly metadata)
- [CRoaring](https://github.com/RoaringBitmap/CRoaring) (adaptive bitmap containers)
- [simdjson performance notes](https://github.com/simdjson/simdjson/blob/master/doc/performance.md) (structural SIMD, buffer reuse)
- [Google Highway](https://github.com/google/highway) (portable SIMD)
- [fast_float](https://github.com/fastfloat/fast_float) (exact, high-performance numeric parsing)

The prototype is independently written. These references do not introduce dependencies or imply copied code.
