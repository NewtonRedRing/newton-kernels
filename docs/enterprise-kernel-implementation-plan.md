# NewtonKernels: Implementation-ready enterprise-kernel roadmap

**Status:** Proposed interfaces and delivery sequence; NOT implemented or released.
**Repository:** NewtonRedRing/newton-kernels, `main` audited at `ad847610aafb65f4d6fdc1f9eb32e72c507bb490`, 2026-11-03.
**Companion:** [v1 architecture and guardrails](v1-architecture-roadmap.md).
**Source-of-truth benchmark evidence:** [22 paired cases](../benchmarks/results/github-actions-2026-11-02-a3ccc65/REPORT.md).
**Absolute prerequisite:** [PR #4: performance guard](https://github.com/NewtonRedRing/newton-kernels/pull/4) and [issue #5](https://github.com/NewtonRedRing/newton-kernels/issues/5).

## 1. Outcome, invariants, and non-goals

Goal: a dependency-light C++20 library of reusable, predictable CPU kernels for ingestion/analytics, vector retrieval, explicit concurrency, and observability, **not** a database, distributed system, LLM agent platform or hidden runtime.

Every new feature must preserve:
1. Direct `newton::...` APIs; zero mandatory initialization, daemon, scheduler, JIT, background profiling, network connection or global context.
2. Separate opt-in CMake targets; no mandatory third-party runtime dependencies. The existing eight targets and installed headers remain compatible.
3. Portable scalar implementation. Explicit CPU/OS-aware AOT acceleration may dispatch to AVX2/AVX-512 when safe. Unsupported ISA must never fault.
4. Caller-owned input/output/scratch for every hot `*_into` path; named convenience APIs may allocate and disclose costs. No hidden threads.
5. Precise correctness contracts: bit validity, ownership, aliasing, overflow, output determinism, error behavior and strong/weak exception guarantees documented per operation.
6. All **22 established benchmarks** remain runnable, with exact output checks, each compared on the same host against the PR base and the published source anchor `a3ccc65021181561aec1c8fe284bdeb7c8e1234c`.
7. Published comparisons always show losses, memory costs and work excluded from timers. No universal fastest or absolute "sub-10ms" claim.

Explicit non-goals for first release: HNSW until exact search succeeds; replicated state-machine consensus, payment ledger, SQL server, LLM orchestration, embedded CUDA/PyTorch/Arrow C++/OpenTelemetry SDK, implicit worker pool and arbitrary automatic tuning.

## 2. Existing code that must be extended, not recreated

- `include/newton/analytics/dense_group.hpp` contains allocation-free dense count and count/sum with checked unsigned overflow.
- `include/newton/analytics/flat_hash.hpp` contains `FlatCountSum` and exact duplicate-preserving `flat_hash_join_into`.
- `include/newton/analytics/fused_join.hpp` implements a specialized fused join/aggregate.
- `include/newton/parsing/csv_fields.hpp` and `structural_mask.hpp` have borrowed fields and quotation structure.
- `include/newton/index/inverted_index.hpp` and `hybrid_postings.hpp` provide lexical search and immutable postings.
- `include/newton/parallel/executor.hpp` is an explicitly constructed but unbounded queue/future executor: preserve its API; do not silently replace it.
- `include/newton/core/phase_trace.hpp` provides caller-owned phase timing.
- `src/simd/byte_scan.cpp` and `core/cpu_features.cpp` offer runtime-dispatched AOT feature checks.
- `CMakeLists.txt` defines eight independent static targets. `benchmarks/competitors` and the existing CI compare 22 historical cases.

The original benchmark campaign deliberately does NOT prove the new opt-in headers are faster: add independent workload tests before promoting any implementation.

## 3. API contracts (C++20 design sketches, not yet shipping)

### 3.1 Columnar data and validity

Files: `include/newton/columnar/{view,validity,selection}.hpp`. CMake target `NewtonKernels::columnar` begins as an `INTERFACE` header-only package. No globals or heap allocation.

```cpp
namespace newton::columnar {
struct ValidityBitmapView {
    std::span<const std::uint8_t> bytes; // packed bits, LSB-first
    std::size_t bit_offset = 0;         // first logical row
    std::size_t length = 0;
    bool is_valid(std::size_t row) const;  // bounds checked
};

template<class T>
struct ColumnView {
    std::span<const T> values;            // borrowed, same lifetime as caller
    std::optional<ValidityBitmapView> valid; // nullopt means all rows valid
    std::size_t size() const noexcept { return values.size(); }
};

// All row IDs are u32 for v1. A checked error is required above UINT32_MAX.
std::size_t select_valid_into(
    ValidityBitmapView mask, std::span<std::uint32_t> row_ids);
}
```

**Semantics:** validate `bit_offset + length` and buffer capacity with checked arithmetic; no out-of-bounds partial-byte reads; null rows ignored in selection; empty mask is valid; borrowed buffers must outlive every call. For hot selection, output must have `capacity >= logical_rows` and insufficient capacity throws before writing. Aliasing input/output is forbidden unless documented. No implicit ownership or normalization.

### 3.2 Fused analytics, deterministic arithmetic

Files: `include/newton/analytics/fused_aggregate.hpp`, `src/analytics/fused_aggregate.cpp`, `tests/fused_aggregate_tests.cpp`, `benchmarks/fused_pipeline_bench.cpp`.

```cpp
namespace newton::analytics {
std::uint64_t filter_sum_gt_u64(
    columnar::ColumnView<std::uint64_t> values,
    std::uint64_t exclusive_lower_bound); // checked overflow, null ignored

std::size_t filter_greater_i64_into(
    columnar::ColumnView<std::int64_t> values, std::int64_t threshold,
    std::span<std::uint32_t> selected); // output capacity >= rows

void dense_group_count_sum_if_ge_into(
    columnar::ColumnView<std::uint32_t> keys,
    columnar::ColumnView<std::uint64_t> values,
    std::uint64_t minimum_value,
    std::span<std::uint64_t> counts, std::span<std::uint64_t> sums);
}
```

**Semantics:** same row cardinality, null row excluded if either column is null, inclusive group domain [0,counts.size), no silent arithmetic wrap, output groups are sorted by key by virtue of direct indexing, counts/sums pre-zeroed; integer overflow throws `std::overflow_error`. Validation errors leave outputs untouched; arithmetic overflow may leave a partial result (explicitly documented). Existing `dense_group_count_sum_into`, `group_count` and `hash_inner_join` unchanged. Start with a scalar oracle; add ISA-specific implementations only when measured.

### 3.3 Vector distance and exact top-K

New independently linkable `NewtonKernels::vector`, optionally links `core` but never `parallel`. Files:
- `include/newton/vector/{view,distance,topk,flat_index}.hpp`
- `src/vector/{distance,flat_index}.cpp`
- `tests/{vector_distance,vector_search}_tests.cpp`
- `benchmarks/{vector_distance,vector_search}_bench.cpp`

```cpp
namespace newton::vector {
enum class Metric { SquaredL2, CosineDistance, NegativeDot };
struct MatrixF32View {
    std::span<const float> data; // borrowed dense matrix, row-major
    std::size_t rows, dimensions, stride_elements;
};
struct ScoredId { std::uint32_t id; float distance; };

float dot_f32(std::span<const float> x, std::span<const float> y);
float squared_l2_f32(std::span<const float> x, std::span<const float> y);
float cosine_distance_f32(std::span<const float> x, std::span<const float> y);

std::size_t exact_topk_into(
    MatrixF32View matrix, std::span<const float> query, Metric metric,
    std::size_t k, std::span<ScoredId> output,
    std::span<ScoredId> workspace); // caller owns all memory
}
```

**Semantics:** `rows * stride` bounds and `dimensions <= stride` validated without overflow. Empty matrix and k=0 return zero; reject zero-dimensional, mismatched or nonfinite vectors; zero-norm cosine throws `std::domain_error`. All metrics normalized to *lower distance is better*: negative dot for dot-product ranking. Order ascending by distance then ascending ID for ties; `k <= rows` or clamp explicitly (decision: clamp to rows). Exact search is exhaustive over all rows; deterministic tie ordering and checked uint32 IDs. Select a Float32 accumulation order or double-precision scalar reference and specify tolerance (`abs_error <= 1e-4 + 1e-4 * abs(reference)` in screening tests); benchmark FP error separately. SIMD paths may differ by permitted tolerance, not rankings outside declared near-tie ambiguity.

Test flat scanning first with a caller workspace of at least k. A compiled exact-search oracle protects any later ANN index. HNSW is a **separate deferred PR**, not part of exact-search MVP.

### 3.4 Explicit bounded concurrency

Keep `parallel::Executor` public and behavior-compatible. New opt-in `parallel::BoundedExecutor` target implementation can remain within the existing `parallel` library; do not create threads until user constructs it. Files `include/newton/parallel/bounded_executor.hpp`, `src/parallel/bounded_executor.cpp`, `tests/bounded_executor_tests.cpp`.

```cpp
namespace newton::parallel {
enum class SubmitResult { Accepted, Full, Stopped };
class BoundedExecutor {
public:
    BoundedExecutor(std::size_t workers, std::size_t queue_capacity);
    ~BoundedExecutor() noexcept; // defined drain-and-join policy
    BoundedExecutor(const BoundedExecutor&) = delete;
    BoundedExecutor& operator=(const BoundedExecutor&) = delete;
    SubmitResult try_submit(std::function<void()> task);
    void request_stop() noexcept; // reject new work; drain existing work
    std::size_t queue_capacity() const noexcept;
};
}
```

`try_submit` is nonblocking with respect to queue capacity; allocation/callback copy can still allocate, and this must be stated. `workers` and `queue_capacity` must both be positive. All callbacks finish before destruction returns; no detached tasks. Exceptions from tasks must be captured in a caller-visible exception channel (design decision required before merging, e.g. explicit future-producing overload or callback). Avoid recursively submitting from a worker into a full queue. A separate synchronous batch API can deliver allocation-aware `parallel_for_into` with a caller scratch pool and a `std::stop_token`. Measure versus a persistent pool with equivalent lifetime, not one-off `std::async` tasks.

### 3.5 Opt-in observability

Extend existing `core/phase_trace.hpp` without removing `ScopedPhase`. New file `include/newton/core/metrics_sink.hpp`. No global registry, sampler or OpenTelemetry dependency.

```cpp
namespace newton::core {
struct MetricsSink {
    void* context = nullptr;
    void (*record_duration)(void*, std::string_view, std::uint64_t) noexcept = nullptr;
};
void record_duration(
    const MetricsSink* sink, std::string_view phase,
    std::uint64_t elapsed_ns) noexcept;
}
```

The application explicitly owns and outlives the sink and histogram storage. Callbacks must be noexcept and may not reenter a non-reentrant metrics collector; disabled paths require one cheap null check and no allocations. Thread safety is the sink implementer's responsibility or an explicit opt-in adapter; phase labels must outlive any deferred recording. Export runtime data via user-provided sink, not implicit network requests.

## 4. Proposed file/target map

| Existing or new target | Owned change locations | Dependencies |
|---|---|---|
| `core` existing | `include/newton/core/metrics_sink.hpp`; compatible `phase_trace.hpp` | none |
| `memory` existing | existing aligned/mapped views reused | none |
| `simd` existing | AOT functions/dispatch where justified | `core` |
| `columnar` NEW `INTERFACE` | `include/newton/columnar/*.hpp` | none |
| `analytics` existing | new fused aggregate .hpp/.cpp; leave old API stable | `columnar` (public), optionally `simd` |
| `parsing` existing | `columnar_decode.hpp`; optional Arrow C adapter | `simd`, `columnar` for bridge |
| `vector` NEW static | `vector/*.hpp`, `src/vector/*.cpp` | `core`; no implicit `parallel` |
| `index` existing | `snapshot.hpp`, lexical hybrid adapters optional | none, vector hybrid separate |
| `parallel` existing | `bounded_executor.hpp/.cpp` | standard threads |
| `tools/benchmarks` | new load harness, benchmark fixtures, diff checks | optional development-only deps |

**Build integration:** modify root `CMakeLists.txt` to add one target at a time, export/install target with `NewtonKernels::` namespace, link only declared dependencies, and update CMake package consumer example. Keep `NEWTON_BUILD_TESTS` and `NEWTON_BUILD_BENCHMARKS` opt-in. No network during normal configuration. Add GCC/Clang/MSVC install-consumption CI to prove modular linkage.

## 5. Performance contract and fail-closed acceptance gates

**Phase zero gate: no feature PR until PR #4 works reliably and an admin ruleset requires `compare` plus CI to pass.** At creation, `main` was not protected. The first guard run reported time variance in `parallel_tasks` even when kernel code was unchanged; this must be resolved and reported transparently.

For every PR:
1. Build baseline `main`, candidate and fixed published source anchor on one runner, same C++20 compiler, flags, workload seeds and timers; run **all 22 original fixtures** with strict matching checksum/items/bytes. Keep original fixture manifest intact.
2. Run the new feature's *correctness oracle* and randomized differential tests; zero sanitizer findings, illegal ISA faults and undefined behavior.
3. Compare p50 and tail latency, throughput at a stated concurrency, total allocations and peak RSS where testable. Include cold/warm I/O and setup/build costs for full pipelines.
4. Gate repeated established-case regressions (initial guard policy: >3% paired median slowdown in at least 4/5 rounds). **Any positive slowdown, even less than 3%, is a visible warning requiring human review**. This is a noisy-runner detection policy, not a promise that true regressions smaller than 3% are acceptable.
5. Do not publish "faster" claims for new kernels until same-host end-to-end comparisons vs independent tuned C++ STL and appropriate domain baselines (e.g. Faiss exact search for vector), with equal semantics and full data.
6. When the shared runner cannot distinguish noise from true regression, flag inconclusive and rerun on a stable pinned dedicated host with more trials, controlled affinity and unchanged frequency settings. Never bypass correctness checks or conceal a failing case.

**Flagship workload gates** (proposed, not measured):
- CSV->borrow/decode->columnar filter->dense group sum: exact output and no allocation in `*_into` hot path; end-to-end latency strictly nonregressive to sequential reference, otherwise keep optimization optional/off.
- Vector distance: numerical error within declared tolerance and broad scalar/ISA equivalence; latency/throughput gains on meaningful dimensions without breaking short vectors.
- Exact Top-K: recall@K = 100% by definition vs exhaustive oracle; report K=1/10/100, D=128/384/768, N=10K/1M and 10M only on sufficient hardware; p95/p99, build cost, RSS, throughput and query batches. "p99 under 10ms" is an experimental goal bound to a fixture, not universal guarantee.
- Bounded executor: no unbounded queue growth, deterministic bounded submit behavior, correct shutdown/error delivery; benchmark p99 under oversubscription, at fixed 1/4/16/64 clients.

**Noise controls:** alternate run order, independent process samples, multiple seeds and repeated trials, fixed CPU affinity where possible; report bootstrap/confidence intervals or variability. Existing synthetic tests alone are insufficient to establish production-grade results.

## 6. Correctness and safety test matrix

| Domain | Required tests |
|---|---|
| Column views | Empty/one element, arbitrary bitmap offsets including 7/8/9, last byte, short validity buffer, size overflow, null semantics, borrowed views |
| Fused analytics | Scalar differential oracle, 100+ fixed seeds, zero/all/null selectivity, dense/sparse keys, 32/64-bit overflow, count capacity, full duplicate preservation |
| Vectors | D=1/3/7/128/384/768; aligned/unaligned data, stride, floats near zero, NaN/Inf rejected, ties, k=0/k>N, nearly equal scores, AOT fallback |
| Concurrency | TSan where supported; racing submit/shutdown, cancellation, task exceptions, queue saturation, 1/4/16 worker scaling, no hidden threads |
| Observability | Disabled sink overhead, lifetime/label safety, counter overflow policy, monotonic timestamps, callback reentrancy policy, histogram accuracy |
| Portability | Linux GCC/Clang, macOS Clang, Windows MSVC; x86 scalar/AVX2/AVX-512 and ARM scalar first; MSVC unsupported ISA falls back safely |
| Packaging | Out-of-tree CMake `find_package` linking only one module, install, ABI/headers check, no implicit new library dependency |
| Regression suite | Every original case retained and output-equivalent; monotone performance policy as above; raw artifact preservation |

ASan/UBSan on every new module; fuzz invalid CSV/column bitmap and vector sizes; separate TSan job on supported hosts. Test and document exception safety and partial mutation; results are never silently truncated.

## 7. Prioritized GitHub issues and dependency graph

**P0 — must land first**
- [#5 Required regression guard and branch ruleset](https://github.com/NewtonRedRing/newton-kernels/issues/5) → prerequisite to every following kernel PR.
- [#6 API semantics and benchmark matrix](https://github.com/NewtonRedRing/newton-kernels/issues/6) → gates #7, #8, #11, #13, #14.
- [#7 ColumnView / validity / selection](https://github.com/NewtonRedRing/newton-kernels/issues/7) → prerequisite #8, #10.
- [#8 Fused filter and group/sum](https://github.com/NewtonRedRing/newton-kernels/issues/8) → requires #7.
- [#9 Improve existing group/join baseline](https://github.com/NewtonRedRing/newton-kernels/issues/9) → after #6; can parallel #7/#8 but one algorithm per PR.

**P1 — build new capabilities after correctness gates**
- [#10 Columnar parsing and Arrow C adapters](https://github.com/NewtonRedRing/newton-kernels/issues/10) → #7 and #8.
- [#11 Vector distance kernels](https://github.com/NewtonRedRing/newton-kernels/issues/11) → #6.
- [#12 Exact Top-K flat vector search](https://github.com/NewtonRedRing/newton-kernels/issues/12) → #11.
- [#13 Bounded execution/cancellation](https://github.com/NewtonRedRing/newton-kernels/issues/13) → #6.
- [#14 Explicit telemetry and tail-latency harness](https://github.com/NewtonRedRing/newton-kernels/issues/14) → #6; use for performance gates in #12/#13.

**P2 — conditional research**
- [#15 Immutable snapshots/hybrid retrieval](https://github.com/NewtonRedRing/newton-kernels/issues/15) → #12 and #13.
- [#16 HNSW comparison and possible opt-in ANN](https://github.com/NewtonRedRing/newton-kernels/issues/16) → #12 and #14; promote only when recall/latency/footprint evidence demonstrates value.

## 8. Phased PR sequence and review protocol

| PR | Branch prefix | Scope (one reviewable change) | Dependencies |
|---|---|---|---|
| 0 | `ci/performance-regression-guard` | Complete #5; merge PR #4 only after green and configure mandatory ruleset | none |
| 1 | `docs/contracts-baselines` | #6 input/ownership contracts, seed-preserving benchmark baselines | PR 0 |
| 2 | `feat/columnar-views` | #7 header-only view target, tests/install/export | PR 1 |
| 3 | `perf/fused-aggregates` | #8 scalar fused kernels then measured AOT strategy | PR 2 |
| 4 | `perf/hash-analytics` | #9 group/join improvements; no silent replacement of established implementation | PR 1 |
| 5 | `feat/columnar-parsing` | #10 borrowed CSV bridge and optional Arrow C adapter | PR 2–3 |
| 6 | `feat/vector-distance` | #11 scalar and AOT ISA kernels | PR 1 |
| 7 | `feat/vector-exact-topk` | #12 exact index, workspace, rankings | PR 6 |
| 8 | `feat/bounded-executor` | #13 explicit bounded queue and cancellation | PR 1 |
| 9 | `feat/observability` | #14 sink and open-loop tail-latency harness | PR 1 |
| 10 | `feat/index-snapshots` | #15 concurrent snapshots/hybrid search | PR 7–9 |
| 11 | `research/vector-hnsw` | #16 isolated ANN experiment (merge only if gate passed) | PR 7, 9 |

For each PR: description must include ABI/API contract, before/after table per original case, new-module correctness oracle, sanitizer/test results, build/toolchain info and machine/dataset hashes, RSS/allocations if available, and a go/no-go statement. Squash after review if policy permits. Never merge red checks; do not overwrite the historical baseline. Rebase rather than combining incomparable feature changes. Explicit benchmark regressions require reversing/specializing the fast path, not simply raising the allowed threshold.

## 9. Definition of done

v1 CPU portfolio is complete only when: all existing eight modules and the added opt-in target(s) install independently; all 22 old fixtures and newly accepted workloads remain correctness-equivalent; mandatory CI regression protection is active on `main`; no background runtime, worker or global mutable context has crept in; full benchmark evidence is reproducible; and a real end-to-end ingest/filter/group pipeline and exact vector retrieval application show a defensible technical improvement on declared workloads.

Implementation status at plan publication: **planning and GitHub issue creation only**. No new columnar/vector/concurrency/telemetry production kernels are delivered by this document.
