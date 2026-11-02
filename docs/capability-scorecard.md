# NewtonKernels — capabilities, competitors, and measurement completeness

**Version:** development preview v0.1.0 · **Evidence snapshot:** 2026-11-02 · **Policy:** show every outcome, including competitors' advantages and unmeasured parameters.

## Portfolio overview

| Library | Public capabilities | External benchmark | Current limitation |
|---|---|---|---|
| `core` | CPU feature detection, dispatch helpers | Not yet | No isolated external overhead comparison |
| `memory` | Read-only mmap, aligned buffers | Not yet | Need cold/warm file I/O and allocator study |
| `simd` | Scalar/AVX2/AVX-512 byte search and count | **Measured: optimized STL `std::count`** | Counting only, no vectorized search comparison |
| `parsing` | Quoted CSV; borrowed, unquoted records | **Measured: Vince's CSV Parser** | In-memory, single-thread; Vince has additional features and parallel I/O modes |
| `index` | Postings, Boolean AND, phrase search | Not yet | ASCII tokenization and no persistence/Unicode support |
| `analytics` | Group count, joins, exact percentiles | Not yet | Internal sort vs selection comparison only |
| `graph` | CSR, radix/binary Dijkstra, reusable workspaces | **Measured: Boost.Graph** | Shortest-path query only; graph build excluded and workspace policies differ |
| `parallel` | Bounded task executor | Not yet | Need thread-count scaling, tail latency, contention benchmarks |

## External comparison parameters

All six current external benchmark pairs use deterministic same-seed inputs, a single runner/compiler, repeated runs, and matching output hashes. Reported per-pair metrics:

| Parameter | Available? | Trustworthy interpretation |
|---|---|---|
| Latency / time per operation | **Yes** | Median of independent process medians, lower is better |
| Throughput | **Derived** | Same timed work units divided by measured latency; higher is better |
| Speedup | **Derived** | Competitor time ÷ Newton time. Below 1.0× means competitor wins |
| Per-process timing spread | **Yes** | Individual process timing medians in raw JSON |
| Correctness / output digest | **Yes** | Equivalent digest and item counts on each matched fixture, not a formal proof |
| Peak process resident memory | **Captured** | Whole-process measurement includes shared fixtures/runtime, **not an isolated library-memory comparison** |
| Input sizes | **Yes** | Two sizes per measured operation, no exhaustive scaling study |
| Build flags / compiler / operating system | **Yes** | Recorded in environment and workflow; implementations may have different internal codegen |
| CPU affinity / governor / virtualization | **Partially** | Affinity and virtualization recorded; governor not available on CI |
| CPU counters, cycles, IPC, cache misses | **No** | Must not invent numbers |
| Power, energy efficiency | **No** | Requires suitable measurement hardware |
| Cold cache / cold start | **No** | Current tests are warm repeated kernel measurements |
| Thread scaling, contention, tail latency | **No** | Current external cases do not establish multicore scaling |
| Graph build, CSV file I/O and output writing | **No** | Excluded in current timed regions |
| Binary size, compile time, build dependencies | **Not externally benchmarked** | Separate install/build quality assessment needed |
| API/feature parity | **Not fully assessed** | Competing libraries may provide broader capabilities |
| Cross-architecture performance | **No** | Only one public CI x86-64 external comparison available |

## Public evidence

- [Interactive showcase](../showcase/index.html) (serve locally; deployment supported via GitHub Pages workflow).
- [Raw matching-input results](../benchmarks/results/github-actions-2026-11-02-5889b44/results.json).
- [CI-run summary](../benchmarks/results/github-actions-2026-11-02-5889b44/REPORT.md) and [official run record](https://github.com/NewtonRedRing/newton-kernels/actions/runs/41484043676).
- [Exact adapter implementation and fairness notes](../benchmarks/competitors/README.md).
- [Internal strategy benchmarks](performance-comparison.md) are **separate** from external-library measurements.

## How to extend without cherry-picking

For each module, choose representative datasets (small, medium, large; uniform/skewed/adversarial), define exact equivalent output, add independent code adapters, run alternating randomized pairs, report every result including regressions, and track time, RSS, allocations and hardware counters if available. Compare single-thread and fixed-N thread configurations separately. On any failing/unsupported case, report **not comparable**, not a zero or a win.

The interactive site filters on observed results and has no special case suppressing slower NewtonKernels outcomes. The current verified runs happen to favor NewtonKernels on time; that does **not** establish overall superiority across every capability.
