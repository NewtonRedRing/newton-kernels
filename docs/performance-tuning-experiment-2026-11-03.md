# Performance tuning experiment — 2026-11-03

Baseline: main at `b33078d3adce6f8979b9b22fbdc75a3a2f040fd3`.

## Changes

- Cache process-usable x86 CPU feature flags exactly once, using C++11 thread-safe static initialization. Other platforms retain the all-false feature snapshot. This avoids repeated `__builtin_cpu_init()` calls on hot paths.
- Replace one heap-allocated `std::unordered_multimap` node per right row in the general hash join with an `unordered_map` of per-key contiguous vectors of borrowed pointers. Keys use full equality and all duplicates are preserved. A bounded (2048-bucket) reserve hint avoids oversizing the table for mostly duplicate keys. No central runtime or new external dependency.
- Add independent randomized multiset join tests (128 trials), full string keys, and CPU feature equivalence tests.

## A/B screening (not official benchmark evidence)

A small independently compiled x86-64 C++20 -O3 prototype with the original and candidate join algorithms showed lower median runtime for the published 3K/512 and 12K/2048 synthetic shapes. Performance is topology, compiler and workload dependent, so **no guaranteed speedup is claimed**.

## Acceptance gates

Do not merge unless all Linux/Windows/macOS builds, sanitizers, fuzz smoke, checksum-matching external benchmarks and `regression-guard` (22 established + 6 CSV pipeline cases) pass on the exact PR commit. Inspect every warning, including weak cases such as join, group, feature checks, and memory. No confirmed performance regression may be merged. If the multi-platform or regression evidence does not pass, revise or revert this experiment.

The full future optimization matrix also needs explicit p95/p99, IPC, LLC misses, allocations, RSS, NUMA, and realistic end-to-end datasets. Those are **not** inferred from median microbenchmarks.
