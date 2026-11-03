# NewtonKernels v1: Architecture, Performance Strategy, and Implementation Roadmap

> **Status:** PROPOSAL / DESIGN ONLY — not implemented or approved for release.
>
> **Audit baseline:** NewtonRedRing/newton-kernels, main at `60be8ca898b307492b2109b305df2841a76c8ae0` (2026-11-02).
>
> **Decision owner:** project maintainers. **Document scope:** architecture, APIs, priorities, testing, and release gates. This document does not change code, CMake targets, benchmarks, or current public APIs.

## 1. Executive decision

Make NewtonKernels a **zero-central-runtime, dependency-light collection of hardware-aware C++20 execution primitives**, with an initial specialization in **CPU-native, data-intensive workloads**. The primary value proposition is **minimum correct end-to-end execution time with minimal integration changes**, not a universal claim to be the fastest at every problem.

Keep the existing independent static modules. Add small, composable, caller-controlled primitives rather than an always-running engine. Let each operation select an ahead-of-time compiled strategy using cheap, local CPU and workload checks. Preserve explicit strategy selection for experiments and advanced users. Do not create a mandatory global context, scheduler, JIT, daemon, external service, or runtime calibration stage.

**Flagship v1 demonstration:** process a realistically sized structured-data stream into filtered/grouped aggregates with fewer allocations, less memory traffic, and lower end-to-end time than credible alternatives **on the same machine with equivalent semantics**. Keep index and graph as independent products with their own improvements and measurements; do not gate the whole release on becoming a database or HPC platform.

### Non-negotiable architectural invariants

1. **Direct API:** users can invoke a kernel with ordinary C++ arguments, without initialization or shutdown.
2. **No central runtime:** no process-wide task manager, global worker pool, plugin manager, background service, mandatory context, JIT or hidden autotuning.
3. **No implicit parallelism:** sequential is the default; workers are constructed only through an explicit executor or parallel API/option.
4. **AOT specialization:** a portable compiled baseline and independently compiled ISA-specific variants. Never execute an instruction unsupported by both CPU and OS state.
5. **Opt-in allocation control:** simple convenience APIs may allocate and must say so; performance-sensitive `*_into` APIs accept caller-supplied output and scratch memory with documented capacity requirements.
6. **Correctness before speed:** algorithm choice must not change documented answers, error behavior, integer overflow handling, or null semantics.
7. **No universal speed claims:** report every tested outcome, including competitor wins, with comparable workloads, exact revisions, machine and sample metadata.
8. **Independent code:** competitor implementations may be benchmark dependencies under their licenses; NewtonKernels implementations must be independently authored, not copied from contest entries.

**Clarification:** "zero-central-runtime" is not "zero execution overhead" or "no C++/OS runtime libraries." C++ library initialization, function-local statics, allocation when requested, OS mapping, and explicit worker threads are permitted.

## 2. Current repository audit

### Examined implementation and evidence

- Build / packaging: `CMakeLists.txt`, eight `newton_<module>` STATIC libraries and CMake exports.
- CPU/SIMD: `src/core/cpu_features.cpp`, `src/simd/byte_scan.cpp`, public scanner APIs.
- Memory/parse: `include/newton/memory/aligned_buffer.hpp`, `src/memory/mapped_file.cpp`, `src/parsing/csv_parser.cpp`, `include/newton/parsing/delimited_reader.hpp`.
- Index/analytics: `src/index/inverted_index.cpp`, `include/newton/analytics/group_by.hpp`, `hash_join.hpp`, `src/analytics/percentile.cpp`.
- Graph/parallel: `src/graph/graph.cpp`, `radix_heap.hpp`, `src/parallel/executor.cpp`.
- Evidence: `benchmarks/results/github-actions-2026-11-02-a3ccc65/REPORT.md` (22 paired CI cases), `docs/capability-scorecard.md`, existing unit/integration tests. Audit is source inspection, **not** a fresh performance measurement or exhaustive proof of no global constructors.

| Area | Observed state | Risk / opportunity | Proposed v1 disposition |
|---|---|---|---|
| Library linkage | Separate static CMake targets; no mandatory third-party runtime packages | Good foundation; header-only helpers sometimes allocate | **Preserve** modular packaging |
| Global runtime | No global execution manager or implicit worker pool found in inspected sources | Normal process-local static initialization is not a daemon | **Ban** future hidden central state |
| Default SIMD | `find_byte` / `count_byte` store a process-local, function-static dispatch pointer on first call | Cheap subsequent calls; first call has one-time feature detection | **Keep** lazy, thread-safe once-per-entry dispatch |
| Explicit SIMD | `*_using` calls `select_find` / `select_count` every time; each probes `cpu_features()` | Repeated builtins / dispatch work in explicit benchmark paths | **Plan** cached immutable capabilities and direct strategy entry |
| CPU detection | `cpu_features()` calls `__builtin_cpu_init` and feature checks per invocation | Repeated CPU-probe cost; MSVC currently gets limited x86 detection | **Plan** once-per-process snapshot with explicit refresh only if needed |
| Quoted CSV | `CsvParser` creates owned strings, row vector, decoded escape buffers; callback uses `std::function` | Allocations and type-erasure overhead; full RFC-like semantics require escaping | **Add** borrowed/streaming batch and templated callback paths; retain strict parser |
| Unquoted records | `for_each_delimited_record` borrows string views but builds/grows a field vector per call | Not completely allocation-free for field descriptor storage | **Add** bounded caller-scratch variant |
| Group count | `std::unordered_map<Key, size_t>` without explicit reserve in helper | Little differentiation; skew / dense key ranges can be faster with different layouts | **P0** specialized counting and grouped sums |
| Hash join | `std::unordered_multimap<Key, const RightItem*>` | Pointer chasing, node allocations, arbitrary build-side choice | **P0** flat/partitioned join with duplicate-preserving semantics |
| Percentile | Copies entire input into vector each call; then `nth_element` or sort | Extra allocation and memory traffic; repeated queries repeat copy | **Add** in-place and caller-scratch variants; keep immutable convenience API |
| Inverted index | Per-token strings, vectors of postings/positions; search tokenization allocates; phrase search creates per-document pointer vectors | Query/build allocation pressure and weak locality | **P1** compact postings and reusable query workspace; preserve ASCII contract |
| Shortest path | Reusable workspace available, but convenience API constructs a new O(V) workspace for each query; a choice branch remains inside relaxation loop | Workspaces favor multiple queries; heap-specific specialization may reduce branching | **P1** workspace/documentation improvements and specialization trials |
| Executor | Explicit construction creates threads; task submission uses packaged task, shared allocation, future, deque and mutex/condition variable | No background threads by default; notable per-task overhead | **P1** opt-in coarse-grained parallel primitives; improve scheduler only if justified |
| Hardware specialization | GCC/Clang x86 function-target attributes for AVX2/AVX-512; other paths scalar | ARM64 NEON and MSVC multiversioning have incomplete parity | **P1** per-compiler AOT architecture matrix |
| Testing | Differential graph/index cases, basic integration, 22 matched-input CI competitor pairs | Missing comprehensive fuzz, threading, allocation, tiny-input, cold I/O coverage | **P0** stronger correctness and measurement infrastructure |

**Important evidence boundaries:** The published 22-case GitHub Actions report uses synthetic workloads on an AMD EPYC VM. Five-module "STL" comparisons are independently written reference implementations, not necessarily best-in-class third-party libraries. The parallel reference uses batches of `std::async` versus a persistent four-worker Newton executor: useful lifecycle evidence, **not** proof NewtonKernels beats tuned persistent pools. Group-by at 200K rows was approximately tied and hash join at 12K rows lost to its reference; prioritize realistic improvement instead of claiming every module wins.

## 3. Target user, use cases, and exclusions

### Initial users

Native C++ developers building log processors, data importers, analytic services, search/index subsystems, in-process storage systems, and computational backends that already own their memory and threads. Later: Rust and Python through explicitly built bindings.

### Three flagship workloads for v1 validation

1. **Structured ingestion:** CSV / delimited data -> validate -> project columns -> filter -> grouped count/sum. Measure complete file-to-answer work including decoding and required allocations.
2. **In-memory analytics:** primitive and string-key filtering / grouping / joins with varied skew, cardinality, null distributions and sizes. Compare with optimized STL, a credible hash library, and an appropriately equivalent analytical engine API where practical.
3. **Repeated query workloads:** build index or graph once, then run many search / shortest-path queries. Separate construction, query, and amortized total costs.

### Non-goals for v1

No mandatory SQL parser, storage engine, database server, distributed scheduler, MPI abstraction, GPU runtime, CUDA/HIP dependency, cross-process IPC, automatic file-cache control, interpreter, compiler/JIT, or large plugin framework. Optional integration with Arrow and oneTBB/OpenMP belongs behind isolated targets; neither becomes a dependency of core kernels. GPU and NUMA-specialized execution are research tracks, not v1 shipment gates.

## 4. Package architecture and dependency boundaries

~~~
C++ application, existing memory and thread ownership
    |
    +-- NewtonKernels::core       (CPU capability snapshot; lightweight policy types)
    +-- NewtonKernels::memory     (aligned storage; mapped bytes; opt-in scratch)
    +-- NewtonKernels::simd       (byte scan, bit / primitive filter kernels)
    +-- NewtonKernels::columnar   (PROPOSED: borrowed views, validity, selections)
    +-- NewtonKernels::parsing    (strict CSV; borrowed decode; batch adapters)
    +-- NewtonKernels::analytics  (filter, group, reduce, join, order statistics)
    +-- NewtonKernels::index      (posting lists and reusable query scratch)
    +-- NewtonKernels::graph      (CSR, optional caller-owned shortest-path scratch)
    +-- NewtonKernels::parallel   (only explicit thread/executor operations)
    |
CPU, caches, RAM, and OS functions of the caller's process
~~~

The `columnar` module can begin as an **INTERFACE/header-only view target**, with no process state or binary. Core remains free of parsing, graph, indexing, and parallel dependencies. No kernel should require linking the entire portfolio. Model CMake dependencies one-way and audit installed-package consumption.

Module composition must be at compile time or via ordinary direct calls. The optional "pipeline" feature, if added later, is a **caller-owned, synchronous local batch plan**, not a central runtime or mandatory executor.

## 5. Proposed public API — v1 design sketches, not implementations

### A. No-context direct function call (required)

~~~cpp
// Proposed v1 API. These names and types do not yet exist.
std::span<const std::int64_t> values = /* caller-owned */;
std::span<std::uint32_t> selected = /* capacity >= values.size() */;

auto r = newton::analytics::filter_greater_into(
    values, 100, selected);             // sequential; default strategy
if (!r.ok()) { /* inspect status */ }
// selected.first(r.written) are valid row indices
~~~

A simple allocating overload may coexist with a caller-buffer overload:

~~~cpp
auto hits = newton::analytics::filter_greater(values, 100);
auto r = newton::analytics::filter_greater_into(
    values, 100, output, newton::FilterOptions{.strategy = newton::Strategy::Avx2});
~~~

**Required contract:** generic calls operate without setup and never create worker threads. Explicit unsupported ISA requests either return `UnsupportedStrategy` or use a named fallback policy; never silently execute an illegal instruction. Distinguish automatic fallback from explicitly forced strategy behavior.

### B. Columnar borrowed views (proposed)

- `ColumnView<T>`: `std::span<const T> values`, optional `ValidityBits` with explicit logical `bit_offset`, and a size invariant; no ownership or allocation. Fixed-width numeric types first.
- `StringColumnView`: borrowed bytes and 32-bit or 64-bit offset spans; require monotonically increasing offsets and valid final boundary; document encoding (raw bytes vs UTF-8) and exact comparison semantics.
- `SelectionView`: either sorted row IDs in `std::span<const uint32_t>` or an explicit packed bitmask. A representation tag avoids accidental bitmap/index confusion.
- `RecordBatchView`: a collection of consistently sized column views supplied by the caller. Avoid recursive heap-owned schemas in the hot path. Metadata can be validated once at a public boundary.
- `Mutable*View` and `ScratchSpan`: explicitly controlled output/scratch ownership. No hidden arena or mandatory allocator.
- For >2^32 rows, use 64-bit indices or process independent bounded batches. Avoid silently truncating row counts.

**Semantics to specify before implementation:** null propagation in filter/group/join; whether invalid row bytes may be read; integer overflow policy (checked or documented modular arithmetic); signed zeros, NaNs and comparisons; equality/hash consistency; string collation (v1 bytewise); output ordering; duplicate joins; aliasing restrictions; endian/layout; exception versus status boundaries. Arrow interoperability should follow its stable C Data Interface, including ownership/release rules and non-zero array offsets, but be an opt-in adapter, not a mandatory dependency.

### C. Allocation and scratch contracts

- Convenience functions may return `std::vector` / owned results with documented allocation behavior.
- Performance functions use `*_into`, caller capacities, and typed scratch descriptors; report `OutputTooSmall` with required size when knowable.
- Permit two-pass counting or bounded retry for variable-output kernels (filters, joins).
- Workspace reuse is **local, explicit and not thread-safe unless stated**. Different threads may use different workspaces without global contention.
- No hidden `thread_local` large scratch, unbounded caches, global mutable arenas, or process-lifetime memory leaks.
- Avoid `std::expected` as a required public type because v1 targets C++20, not C++23.

### D. Parallelism is strictly opt-in

~~~cpp
// Proposed; not implemented.
auto result = newton::analytics::group_sum(keys, values); // sequential
newton::parallel::Executor workers(4);                  // user owns threads
auto result2 = newton::analytics::group_sum(
    keys, values, newton::ParallelOptions{.executor = &workers});
~~~

An optional non-owning parallel callback adapter can support a caller's OpenMP/oneTBB pool later. Never create a default hidden thread pool. Handle oversubscription, nested calls, exception propagation, cancellation, and deterministic aggregation as part of explicit contracts.

### E. No mandatory pipeline runtime

The first v1 proof can be a direct fused kernel or small callable adaptor, for example **project + filter + group/count/sum** over one batch. Do not invent a general query optimizer in v1. Any later plan builder stays stack/caller-owned, runs synchronously by default, uses no global registration, and has a documented setup cost.

## 6. Columnar execution and fusion design

**Initial data model:** fixed-width arrays, 64-byte-friendly contiguous scans, optional validity bitmap, bounded batch sizes (evaluate 1K / 4K / 16K / 64K rows), optional selection indices. Support regular, highly skewed, and sparse-validity distributions.

**Operator contract:**

1. **Scan/filter:** produce bitmap or row IDs, selected based on cardinality. A direct fused filter+sum avoids materializing a selection when not needed.
2. **Projection/decode:** borrow unescaped fields; decode escaped CSV fields into caller-owned or explicitly allocated buffers; validate malformed input. Never promise full quoted CSV zero-copy for values that inherently require unescaping.
3. **Group/aggregate:** choose dense direct-address counting for bounded integer domains, open-addressed hash for sparse keys, and partitioned aggregation when justified. Offer count/sum and optional min/max first.
4. **Join:** choose small-build-side flat hash, duplicate-preserving bucket storage, and optional radix partitioning for larger cases. Evaluate high-duplicate and no-match inputs. Respect output-size explosion.
5. **Fusion:** start with one or two hand-coded combinations measured against unfused operation chains; no generic expression interpreter.

**Memory design:** reuse caller-owned buffers, reserve output capacity when estimated, hot/cold separation, SoA for frequently accessed keys/values, compact index types when safe, cache-blocking only when supported by measurements. Choose explicit scratch layouts and avoid unnecessary sorting.

**Dispatch input:** row count, estimated key cardinality (when already known), null density (cheap metadata), output selectivity (if already available), and CPU capabilities. Do **not** pay O(N) sampling cost for O(N) kernels unless expected savings amortize it.

## 7. Algorithm portfolio and selection policy

| Family | AOT variants to evaluate | Selection signals | Avoid |
|---|---|---|---|
| Byte scan | scalar, AVX2, AVX-512BW, ARM NEON | size, CPU, target byte, machine bandwidth | assuming widest SIMD always wins |
| Primitive filter | scalar branch, branchless, vector mask, bitmap output | size, selectivity, output format | producing intermediate arrays unnecessarily |
| Group count/sum | tiny sorted/dense table, flat hash, radix partition | key width, cardinality, skew, batch size | always using node-based `unordered_map` |
| Join | nested-loop for tiny, flat hash, partitioned hash | build size, match multiplicity, memory budget | ignoring duplicates or output growth |
| Exact percentile | in-place `nth_element`, scratch selection, full sort, multiple-quantile select | mutation permission, query count, N | unavoidable copy per one-shot query |
| Text search | sorted intersection, galloping/skip, optional bitmap | posting lengths, density, phrase need | unconditional heap allocation per candidate |
| Graph | binary Dijkstra, radix Dijkstra, BFS for unit weights; optional bidirectional after proof | weight class, topology, source/target workload | violating overflow or shortest-path semantics |
| Parallel tasks | serial / chunked explicit parallel | work per task, cores, queue depth | spawning threads implicitly for tiny jobs |

**Auto policy:** deterministic baseline decision tables with measured thresholds, versioned by architecture/compiler. Default must be correct on unfamiliar CPUs even without any profile. Explicit strategy selection remains available; expose a lightweight "selected strategy" diagnostic API for users, not an always-on profiler.

**Offline autotuning:** an optional standalone command/tool may generate a local immutable threshold header or profile file for a *subsequent build*, with version and CPU/compiler identifiers. Importing or executing a profile is opt-in. The library must never benchmark itself at application start or download models.

## 8. CPU, compiler, and portability contract

- **Baseline:** C++20 scalar on Linux/Windows/macOS; GCC, Clang, MSVC. No baseline binary may assume AVX2 or AVX-512.
- **x86-64:** separate AOT target functions or translation units for SSE4.2, AVX2, AVX-512F/BW where warranted; check hardware capability **and** OS-managed vector state. Allow explicit strategies for reproducible tests.
- **ARM64:** implement and test a NEON path for high-value scan/filter kernels. SVE is exploratory and must not block v1.
- **MSVC:** build separately flagged ISA object files where function-target attributes are unavailable; ensure baseline code does not accidentally inherit the advanced instruction-set flag.
- **AOT packaging:** default one portable library with scalar fallback + selected optimized object variants. Optional `native` builds may target one known CPU. Pin compiler flags and report them.
- **Dispatch cost:** use function-local immutable pointer (or compile-time strategy) initialized at first relevant call. A cached CPU snapshot is process-local data, not a service. Never re-probe CPU in inner loops. For tiny input sizes, inline scalar can beat an indirect call; benchmark both paths.
- **Compiler optimization:** separate PGO/LTO/BOLT experiments where supported; no hidden toolchain requirement. Benchmark cold start, library/code size and incremental build time; reject marginal speedups with disproportionate deployment costs.
- **Memory:** document map lifetime, alignment, allocator behavior and ownership; future NUMA and page hints are opt-in, never mandatory.
- **ABI:** v1 guarantees source compatibility within documented public interfaces; cross-compiler C++ ABI compatibility is **not promised**. Avoid exposing implementation-private node types in stable binary interfaces where practical.

Google Highway is a relevant example of independently developed static/dynamic dispatch tradeoffs, not a required dependency. Arrow C Data Interface is the preferred optional interoperability standard.

## 9. Correctness, security, and stability gates

Before accepting optimized implementations, require:

1. A separate, obviously correct reference implementation and differential tests over multiple seeds and distributions.
2. Boundary tests: zero/one elements, unaligned addresses, page-end scans, length overflow, invalid CSV, escaped quotes/newlines, empty strings, all-null/all-valid batches, max duplicates, 32/64-bit integer boundaries.
3. Sanitizer coverage (ASan/UBSan, and TSan where available/appropriate); no illegal instructions on unsupported hardware.
4. Fuzzing for CSV, string/index parsing, SIMD tails and column offset/validity descriptors.
5. Concurrency tests verifying that default APIs spawn **no threads** and share no mutable global scratch; explicit workspaces are clearly scoped.
6. Error-contract tests for output capacity, invalid spans, bad indices, NaN/null handling and overflow. Establish ordering semantics before introducing parallel variants.
7. Test aliasing and lifetime guarantees, including use-after-release for borrowed Arrow/C data boundaries in an instrumented environment.
8. Document any algorithmic prerequisites (e.g., nonnegative graph weights, monotone radix keys, ASCII tokenization).

**Audit-style v1 invariant checks:** static scan for unapproved global constructors, unexpected `thread_local`, implicit `std::thread`/async in sequential targets; runtime OS observation confirming no default worker creation; symbol and dependency inspections for each static library.

## 10. Benchmark methodology: source of truth for adoption claims

### Separate four result classes

- **Microkernel:** operation in hot cache or fixed memory with setup excluded, documented as microbenchmark.
- **Kernel end-to-end:** include allocation, copy, scratch setup and required output materialization.
- **Application end-to-end:** input -> validated final output, include I/O/decoding/build costs as appropriate.
- **Scalability/resource:** 1/2/4/8/... threads, peak process RSS, allocation bytes, context switches, CPU time, hardware counters when granted.

Never combine timings across machines into a single winner ranking. CI VMs provide regression screening; use dedicated documented bare-metal hardware for publication-quality headline performance claims.

### Workload matrix

| Dimension | Minimum experiment classes |
|---|---|
| Input size | tiny (16B/100 rows), 1KiB, 1MiB, 64MiB, ~1GiB where available |
| Key cardinality | 1, 16, 1K, near-unique |
| Key distribution | uniform, Zipf/skewed, adversarial repeated keys |
| Filter selectivity | ~0%, 1%, 10%, 50%, 99%, 100% |
| Validity | no nulls, sparse nulls, dense nulls |
| Data layout | aligned/misaligned, contiguous/strided when supported |
| Queries | one-shot, hundreds of repeat queries, concurrent threads |
| CPU | scalar baseline, AVX2 machine, AVX-512-capable machine, ARM64/NEON |
| I/O | warm-cache and cold-cache **separately**, streamed and mapped |
| Compilation | baseline portable vs architecture-optimized; PGO/LTO separate |

### Rules for competitor comparisons

- Same machine, same physical cores, frequency/affinity policy, memory limits, compiler and compatible architecture flags; exact third-party revision and license recorded.
- Same input bytes and **equivalent semantic contract**, including nulls, quoting, errors, result ordering or canonicalization, and output materialization.
- Include strong relevant comparators: optimized STL and a well-maintained flat-hash library for grouping/join; Vince's CSV Parser for matching parse semantics; Boost.Graph for graph; oneTBB/explicit worker pool for fair parallel scheduling. Avoid treating `std::async` alone as the definitive competitor to a persistent pool.
- Interleave repeated process runs, warm up, publish median plus distribution/confidence interval, and retain all raw samples. Validate full/canonical outputs **before** accepting any timing.
- For each claim report absolute latency, throughput, p95 if relevant, peak RSS and allocation bytes; include cycles/IPC/cache misses or energy only when genuinely measured, never inferred.
- Count all phases for the end-to-end headline. Report startup/dispatch overhead and small-input regressions as first-class results.
- Pin generator, seed, input data hash, compiler, flags, binary revision, CPU model/stepping, OS/kernel, NUMA and thread configuration.
- Website must expose competitor wins, "not measured" values, caveats and reproducible commands. No claims of universal superiority.

Google Benchmark provides useful controls for repetitions, warm-up, random interleaving, counters, and CPU-time versus wall-time experiments. Keep the existing custom differential runner where its process isolation adds value.

### Proposed quantitative gates — acceptance criteria, not observed results

- **Correctness:** 100% of supported differential, fuzz-regression, integration and sanitizer gates pass; no known result differences.
- **Adoption:** a fresh CMake consumer links one kernel without any global setup; installation and documentation tested on Linux/Windows/macOS.
- **Default overhead:** no implicit threads, no hidden runtime service and no mandatory heap allocation in `*_into` fast paths given sufficient scratch.
- **End-to-end value:** aim for at least **1.25x median speedup on two representative, semantics-equivalent workloads** relative to the strongest practical comparator, on controlled hardware; publish failures and do not market target as achieved until reproduced.
- **Regression budget:** no unacknowledged >10% regression on selected production-like workload classes, including small inputs, compared with the prior release.
- **Resource guard:** report process RSS and allocated bytes for each claimed performance win; a speed gain with significant memory cost requires an explicit tradeoff explanation.
- **Reproducibility:** one command rebuilds fixtures, runs the complete comparison, validates output and emits JSON/report/chart with traceable revision metadata.
- **No forced dependency:** building core, simd, analytics or graph must not require downloading competitor packages, MPI or GPU toolchains.

## 11. Workstream ranking

Scoring uses **Impact (I)**, **adoption reach (A)**, **evidence of current weakness (E)** and **implementation effort (C)**, all ordinal 1–5. `priority_score = 2I + A + E - C`. This is an initial planning judgment, not measured ROI.

| Rank | Workstream | I | A | E | C | Score | Why now |
|---:|---|---:|---:|---:|---:|---:|---|
| 1 | Optimized group/aggregate + hash join | 5 | 5 | 5 | 4 | 16 | Existing methods wrap STL; demonstrated join comparator wins |
| 2 | Column views + borrowed CSV parsing | 5 | 5 | 5 | 5 | 15 | Remove allocations and enable fused workloads |
| 3 | Correctness + reproducible end-to-end harness | 5 | 5 | 4 | 4 | 15 | Proof needed before marketing or autotuning |
| 4 | Fused filter + group/sum kernels | 5 | 4 | 4 | 5 | 13 | Best opportunity to beat a sequence of library calls |
| 5 | CPU snapshot + low-cost dispatch / ARM NEON | 3 | 5 | 4 | 3 | 12 | Solves explicit-call overhead and broadens hardware support |
| 6 | Index compact storage + reusable scratch | 4 | 3 | 4 | 4 | 11 | Avoid repeated string/vector allocation |
| 7 | Graph workspace and algorithm selection | 3 | 3 | 3 | 3 | 9 | Already competitive on measured graph cases; targeted upgrades |
| 8 | Explicit parallel scheduling improvements | 3 | 3 | 3 | 4 | 8 | Optimize after useful coarse-grained parallel kernels exist |
| 9 | GPU/MPI/cluster framework | 2 | 2 | 1 | 5 | 2 | Outside v1; heavy maintenance, weak immediate integration payoff |

**Interpretation:** the three highest priorities are concurrent design streams, but the *first implementation* should deliver a benchmarked correct grouped aggregation with a caller-buffer API, then add columnar batch support and fused parsing/filtering once semantics are frozen.

## 12. Gate-driven roadmap (no implementation has started)

### Phase 0 — contracts and baseline (P0)

**Deliverables:** v1 API/semantic RFCs; standardized errors, null/overflow/string/order rules; representative workloads and strong comparators; recorded compiler/hardware results; dependency/threads/allocations audit.

**Exit:** baseline harness and independent correctness oracles run locally and in CI; source and user-visible numbers are reproducible; v1 API decisions documented. No "fastest" claim.

### Phase 1 — zero-overhead foundations (P0)

**Deliverables:** internal cached capabilities, AOT multiversion build layout, nonallocating result/output/scratch APIs, a small columnar view target and correctness tests.

**Exit:** no implicit workers or mandatory context, no unsupported ISA, no unexpected allocation in documented `*_into` paths, installable package on three major OS families.

### Phase 2 — competitive kernels (P0)

**Deliverables:** group count/sum, primitive filter, a performant duplicate-preserving hash join, small-input strategies, and variant selection. Retain STL-compatible convenience APIs.

**Exit:** repeated randomized correctness gates and benchmark reports show which strategy wins or loses by workload. Demonstrated end-to-end benefit, not merely isolated microbench wins.

### Phase 3 — parse and fuse (P0/P1)

**Deliverables:** borrowed/decoded CSV batches with defined lifetimes, direct `filter + group/sum` fusion, representative file-to-answer examples, optional Arrow C Data Interface adapter.

**Exit:** one complete workload runs without global runtime and passes exact output comparison. Measure I/O, decoding, compute, materialization, RSS, and allocations separately and together.

### Phase 4 — refine independent portfolio (P1)

**Deliverables:** compact index/posting strategy, caller query workspaces, graph heap specialization, portable ARM64 SIMD and optional explicit parallel reductions.

**Exit:** no new default threads, correct scalar fallback, architecture CI pass, repeated query benchmarks and no unacknowledged regressions.

### Phase 5 — v1 freeze and adoption (release gate)

**Deliverables:** stable documented source API, semantic versioning policy, CMake exported targets, optional Conan/vcpkg manifest, real consumer examples, negative/positive external comparisons, website and tutorial update.

**Exit:** all acceptance gates in §10 met or explicitly documented as release blockers with honest scope adjustment. Publish release notes listing both wins and limitations. Only then tag v1.0.

**Implementation discipline:** one isolated change per benchmarked experiment; performance baseline -> hypothesis -> implementation -> correctness -> multi-host measurement -> retain/reject. Rejected techniques stay in the research log but never silently contaminate release claims.

## 13. Proposed first 12 engineering tasks (not executed)

1. Write semantics RFC for column views, nulls, output ordering, overflow, ownership and thread safety.
2. Build a proper baseline suite for three full applications with input hashes and strong comparators.
3. Establish "no hidden state" checks: global constructors, background threads, runtime allocations and dependencies.
4. Prototype caller-owned primitive filter/count/sum APIs with scalar reference.
5. Prototype compact flat-hash grouping and dense key-domain variants.
6. Benchmark duplicate-preserving flat/partitioned hash join and memory limits.
7. Prototype borrowed CSV fields with bounded scratch; retain strict owned parser for escaped data.
8. Add fixed-width column views, validity masks and selection vectors without mandatory allocator.
9. Implement and measure a single fused filter+group/sum operation.
10. Cache hardware capabilities once and isolate AOT variants; add NEON test target.
11. Improve index query scratch and shortest-path repeated-query API with immutable graph lifetime.
12. Freeze API, produce a one-page adoption example and publish raw end-to-end performance evidence.

Each task becomes a focused PR with an explicit correctness oracle, measurement plan, and go/no-go decision. **No tasks are authorized or implemented by this document.**

## 14. Open design decisions to resolve before coding

| Decision | Recommended default | Alternative / risk |
|---|---|---|
| Public error handling | Status result for buffer-based `*_into`, exceptions retained for allocating convenience APIs | Mixing errors is confusing unless documented consistently |
| Batch size | Benchmarked 4K–16K default range, not hardcoded into ABI | Smaller/larger depending on cache, row width and selectivity |
| String semantics | Bytewise comparison, explicit UTF-8 validation option | Unicode collation is outside v1 |
| Null semantics | Explicit validity bitmap; invalid values excluded from arithmetic | SQL-style three-valued logic may require separate predicates |
| Output order | Deterministic for documented sequential operations; hash aggregate order unspecified unless asked to sort | Sorting changes time/memory; public flag required |
| Aggregation overflow | Checked integer aggregation in safe API, explicit faster unchecked option only where contract allows | Silent overflow is unacceptable for general-purpose analytics |
| Parallel ownership | Caller-managed Executor reference / callback | Hidden default pool prohibited |
| Scratch ownership | Caller-supplied buffer, move-only optional workspace for convenience | Thread-local implicit pools prohibited |
| Adaptive thresholds | Conservative compiled tables; optional offline tuning | Startup benchmark/JIT prohibited |
| Arrow adapter | Optional format-only C Data Interface target | Do not require Arrow C++ library or change buffer ownership implicitly |
| Release scope | CPU SIMD/columnar analytics and existing modules | Deferring a module's new optimizations is preferable to shallow feature growth |

## 15. References and provenance

**Repository audit:**
- [Project README](../README.md)
- [Existing architecture](architecture.md)
- [Existing optimization principles](optimization.md)
- [Capability gaps](capability-scorecard.md)
- [Verified 22-case comparison](../benchmarks/results/github-actions-2026-11-02-a3ccc65/REPORT.md)
- [Public C++ headers](../include/newton/)
- [Current implementation](../src/)

**External design references, not copied code:**
- [Apache Arrow C Data Interface](https://arrow.apache.org/docs/format/CDataInterface.html) — stable, zero-copy columnar interchange with explicit release lifetimes.
- [Google Highway](https://github.com/google/highway) — AOT SIMD specialization and static/dynamic dispatch tradeoffs.
- [Google Benchmark guide](https://google.github.io/benchmark/user_guide.html) — warm-up, repetitions, random interleaving and counters.
- [oneTBB task arena documentation](https://uxlfoundation.github.io/oneTBB/main/reference/source/task_scheduler/task_arena/task_arena_cls.html) — explicit concurrency constraints; useful comparator for scheduling.

**Documentation policy:** The proposal is a design target. Future APIs, optimizations, ratios, portability, toolchain behavior and acceptance goals must never be represented as current capabilities until merged and measured. All external code, if ever integrated into benchmark-only tools, remains subject to its license and attribution requirements.