# NewtonKernels — complete portfolio capability and benchmark scorecard

**Status:** C++20 development preview. Eight modules have matched-input C++ comparators across two **separate** benchmark campaigns. Five newly tested modules use independent standard-library reference implementations, not third-party package source.

| Module | Scope benchmarked | Comparator | Data availability | Important missing capabilities |
|---|---|---|---|---|
| `core` | Repeated CPU feature detection | Same compiler builtins | **2 local VM pairs** | Target dispatch decision overhead; cross-architecture profiling |
| `memory` | 64-byte aligned storage; mapped-file full scan | Aligned `operator new`; buffered `std::ifstream` | **4 local VM pairs** | Cold-cache I/O; pinned vs pageable memory; NUMA, per-library heap telemetry |
| `simd` | Delimiter count | Optimized STL `std::count` | **2 CI pairs** | Search operations, varying target-byte frequency, small-buffer distributions |
| `parsing` | Quoted CSV decode | Vince's CSV Parser | **2 CI pairs** | Streaming large files, CSV dialect detection, multi-threaded parsing, lifetime API |
| `index` | Two-term Boolean AND; positional build | Independently written positional STL inverted index | **4 local VM pairs** | Phrase query comparison, Unicode, compression, persistent storage, variable term skew |
| `analytics` | Group count and full duplicate-preserving join | Optimized STL hash aggregates and hash join | **4 local VM pairs** | Parallel group/join, larger joins, percentile competitor benchmark, allocation profiles |
| `graph` | Source-to-target Dijkstra on weighted directed graph | Boost.Graph | **2 CI pairs** | Graph build timing, large graph / diverse topology, full-feature algorithm portfolio |
| `parallel` | Four-worker batch tasks | 4-concurrent `std::async` | **2 local VM pairs** | True same-lifecycle thread pool baselines, 1–N thread scaling, queue depth and tail latency |

## All reported parameters

| Parameter | Coverage | Caveat |
|---|---|---|
| Matching checksums and item counts | All 22 cases | Validated outputs for selected fixtures, not formal proof |
| Latency, speed ratios | All 22 cases | Different hosts **never compared against each other** |
| Per-process median / minimum / maximum | All 22 cases | Inner 7 repetitions, alternating outer processes |
| Derived throughput | All 22 cases | Units explicitly reported for each fixture |
| Peak process RSS | All 22 cases | Whole process includes fixture/runtime; not incremental library memory |
| CPU user/system time | 16 local VM cases | Whole process; includes warmup/fixture preparation |
| Minor/major page faults | 16 local VM cases | Whole process, not specifically timed region |
| Voluntary/involuntary context switches | 16 local VM cases | Whole process |
| CPU model / OS / compiler / affinity | Both campaigns | VM governor unknown |
| Binary SHA256 | Both campaigns | A reproducibility identifier, not code size |
| Build times / object-file size | Not measured | Avoid unmeasured comparisons |
| Isolated allocations / malloc traffic | Not measured | Need allocator instrumentation |
| Hardware cycles, cache misses, IPC | Not measured | Requires permitted perf hardware counters |
| Energy/power/NUMA and GPU | Not measured | Hardware support not present |
| Cold-cache and end-to-end application behavior | Not measured | Current synthetic hot-region workloads only |
| Feature/OS parity across dependencies | Limited assessment | No claim of feature-equivalent products |

## Evidence

- [CI results (six cases; established Boost and Vince comparators)](benchmark-dashboard.md)
- [Local five-module report (all 16 cases, both wins and losses)](five-module-benchmarks.md)
- [Reproduction instructions](../benchmarks/competitors/README.md)
- [Interactive website with explicit host selection](../showcase/index.html)

For public claims, cite the operation, input, host, software versions and date. A speed ratio under 1.0× must be shown as a competitor win; no unmeasured parameter should be silently omitted.
