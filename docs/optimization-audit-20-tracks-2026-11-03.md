# Twenty-track algorithm optimization audit — November 3, 2026

> **Resolution status — updated November 3, 2026.** This document is a research
> audit containing hypotheses, correctness risks, and missing measurements,
> **not a list of 20 confirmed defects**. The [track-by-track resolution
> register](optimization-audit-20-tracks-resolution-register.md) gives current
> owner actions and acceptance evidence. The PR #25 transaction fix for
> `SelectiveTermIndex` is **proposed, not merged**, until cross-platform CI and
> the original 22+6 performance guard pass. The older #19–#21 and #23–#24
> proposals are still blocked by regression-guard results. Only the
> caller-owned flat hash specialization from #22 has been merged.
>
> **Required standard for every track:** preserve exact outputs, error behavior,
> portable compilation, and no confirmed performance regression. For a
> performance hypothesis, do not mark it fixed without same-input A/B results.
> A passing check on a different commit is not evidence for this change.

Baseline `main`: `b33078d3adce6f8979b9b22fbdc75a3a2f040fd3`. Each track below was source-inspected concurrently through the connected GitHub repository. This is an **initial source-level audit**, not a mathematical proof that every line has been verified. Changes must be independently tested, benchmarked and reviewed before merging.

| # | Research track | Primary source | Findings / hypotheses | Required correctness gates |
|---|---|---|---|---|
| 1 | CPU feature dispatch | `src/core/cpu_features.cpp` | Repeated CPU init; isolated PR #20; measure heterogeneous CPUs | Compiler feature equivalence; multithreaded access |
| 2 | SIMD scanning | `src/simd/byte_scan.cpp` | AVX2/AVX512 dispatch; test unrolling, tails and downclock effects | Binary/NUL strings, page ends, unsupported ISA |
| 3 | SIMD structural parsing | `include/newton/parsing/structural_mask.hpp` | PCLMUL quote parity; measure short/long block switch | 0–64 bytes, escaped quotes, cross-block parity |
| 4 | CSV field parser | `src/parsing/csv_parser.cpp` | Quoted-field copying and scratch allocation | RFC-like quote/newline/CRLF differential fuzz |
| 5 | CSV record scanner | `include/newton/parsing/csv_records.hpp` | Branch-heavy scalar state machine | Strict grammar/unterminated/multiline records |
| 6 | Numeric CSV aggregation | `include/newton/analytics/csv_numeric_fast.hpp` | Single-pass fused parsing, potential branch/vector tuning | Match strict baseline for every accepted input |
| 7 | General grouping | `include/newton/analytics/group_by.hpp` | Simple unordered map; speculative reserve hurts unknown cardinality | Skewed/uniform/high-cardinality keys and exceptions |
| 8 | Flat hash table | `include/newton/analytics/flat_hash.hpp` | Per-key mix and caller scratch; inspect collision paths | Exact key equality, overflow and table exhaustion |
| 9 | Generic duplicate join | `include/newton/analytics/hash_join.hpp` | Per-row node allocation; separate PR #21 | Complete duplicate pair multiset/string keys |
| 10 | Fused grouped joins | `include/newton/analytics/fused_join.hpp` | Caller-owned outputs, overflow checked | Aliasing contract, count/sum overflow, duplicates |
| 11 | Inverted index | `src/index/inverted_index.cpp` | Token allocation and update exception safety | ASCII normalization, positional AND/phrase, fault injection |
| 12 | Hybrid postings | `include/newton/index/hybrid_postings.hpp` | Dense bitmap vs segmented list crossover | 32-bit universe, boundary IDs and random intersections |
| 13 | Shortest paths | `src/graph/graph.cpp` | Epoch-tagged scratch and algorithm dispatch | Zero weights, overflow, disconnected directed graphs |
| 14 | Radix priority queue | `include/newton/graph/radix_heap.hpp` | Bucket redistribution/locality | Monotone keys, duplicates, reset and exception safety |
| 15 | Thread executor | `include/newton/parallel/executor.hpp` | Callback capture, task queue, nested pool execution | Reentrant calls, teardown, cancellation and exceptions |
| 16 | Memory mapping | `src/memory/mapped_file.cpp` | mmap vs buffered I/O, page faults | Empty/large files, Windows/Linux cleanup |
| 17 | Aligned memory | `include/newton/memory/aligned_buffer.hpp` | 64-byte alignment with allocation overhead; large VM variance | Move ownership, bounds, overflow, zero-size |
| 18 | Columnar kernels | `include/newton/analytics/columnar_kernels.hpp` | Validity bitmap branches and fused arithmetic | Nulls, boundaries, output capacity and overflow |
| 19 | C ABI | `src/c_api.cpp` | Error/status mapping, sum overflow and alias constraints | No exception escapes C, invalid argument safety |
| 20 | Performance methodology | `tools/performance_guard.py` | Paired A/B guard; observe unrelated-case failures in PR #19–21 | Unchanged 22+6 fixtures, no confirmed slowdowns |

## Current concrete algorithm experiment

`flat_hash_join_into` now chooses between a bitmask for power-of-two caller-provided head capacities and unchanged full modulo for other capacities. It performs one dispatch outside both hot loops. Equality tests use exact `uint64_t` keys, including `UINT64_MAX`; every matching pair, including duplicates, is visited. Added 864 independently seeded trials (96 × nine capacities) comparing the full pair multiset against a quadratic oracle.

Local `g++ -std=c++20 -O3 -march=native` paired screening (8K×8K rows, 20 alternating rounds, 70 repetitions) gave baseline/candidate ratios of approximately **1.01× (64 buckets), 1.10× (256), 1.19× (1024), 1.36× (4096), and 0.98× (1009)**. These are not GitHub CI benchmarks and cannot support production speed claims.

## Existing experimental branches and no-regression policy

- Combined CPU feature + duplicate join #19: 22-case regression guard **failed twice** despite join and CPU feature improvements. Do not merge.
- CPU-only #20: all ordinary platform CI checks passed, but an initial paired regression run had failures in unrelated index and join cases; a rerun was requested. Not a proven safe merge.
- Join-only #21: earlier regression run had failures in unrelated CPU, index and parallel-task cases; a rerun was requested. Not a proven safe merge.

The next work should test result stability with independent held-out benchmarks, repeat on a dedicated pinned host, and add evidence for p95/p99 latency, retired instructions, cycles, branch misses, cache misses, RSS, allocator counts and throughput. Any unsupported counter is **not** a zero reading. Never waive a confirmed regression by changing thresholds or protected fixtures.

## Public research references

- Abseil Swiss Tables design (cache-local metadata and probing): https://abseil.io/about/design/swisstables
- Linux perf stat manual (cycles/instructions/cache misses and multiplexing): https://github.com/torvalds/linux/blob/master/tools/perf/Documentation/perf-stat.txt
- Google Benchmark guide (repetitions, medians and coefficient of variation): https://github.com/google/benchmark/blob/main/docs/user_guide.md

These are research references, not copied code, and no external library dependency is added.
