# NewtonKernels

[![CI](https://github.com/NewtonRedRing/newton-kernels/actions/workflows/ci.yml/badge.svg)](https://github.com/NewtonRedRing/newton-kernels/actions/workflows/ci.yml)

**Portable, dependency-light C++20 kernels for data processing, indexing, and graph analytics.**

NewtonKernels provides independently linkable static libraries with stable, documented C++ interfaces and focused correctness tests. Implementations use safe scalar fallbacks and optional runtime-dispatched AVX2 / AVX-512F/BW on supported x86 processors. There are no mandatory third-party runtime dependencies.

> **Status: v0.1.0 development preview.** APIs can change before v1.0. Correctness, tests, packaging and portability are prioritized over unsupported performance claims. This is not yet a distributed, GPU or MPI computing framework.

## Modules

| CMake target | APIs | Notes |
|---|---|---|
| `NewtonKernels::core` | `cpu_features()` | CPU features available to current process |
| `NewtonKernels::memory` | `MappedFile`, `AlignedBuffer` | Read-only mapped I/O, aligned storage |
| `NewtonKernels::simd` | `find_byte()`, `count_byte()`, explicit `ByteScanStrategy` | Scalar / AVX2 / AVX-512 runtime dispatch |
| `NewtonKernels::parsing` | `CsvParser`, `for_each_delimited_record()` | Quoted CSV or zero-copy unquoted lines |
| `NewtonKernels::index` | `InvertedIndex`, `IntersectionStrategy` | Selectable smallest-first / input-order intersection and phrase search |
| `NewtonKernels::analytics` | `group_count()`, `hash_inner_join()`, `percentile_nearest_rank_using()` | Aggregation, exact select / full sort |
| `NewtonKernels::graph` | `GraphBuilder`, `CsrGraph`, `ShortestPathWorkspace`, `shortest_path()` | Selectable radix / binary Dijkstra |
| `NewtonKernels::parallel` | `Executor` | Bounded worker threads and task futures |

## Build & test

Requirements: CMake >= 3.21, a C++20 compiler (GCC, Clang or MSVC), and a thread library. No network access is needed for the build.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNEWTON_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For Ninja users: `cmake --preset dev && cmake --build --preset dev && ctest --preset dev`.

## Install and consume

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNEWTON_BUILD_TESTS=OFF
cmake --build build --parallel
cmake --install build --prefix "$HOME/.local"
```

```cmake
find_package(NewtonKernels 0.1 CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE NewtonKernels::parsing NewtonKernels::memory)
```

Set `CMAKE_PREFIX_PATH` to the installation prefix if needed. See [`examples/`](examples/) and [`docs/getting-started.md`](docs/getting-started.md).

## Columnar data and fused execution

New [borrowed columnar views, Arrow C Data Interface-compatible adapters, and fused CSV aggregation](docs/columnar-and-fusion.md) are provided as synchronous, caller-owned C++20 APIs. `NewtonKernels::columnar` is a header-only CMake target. The optional Arrow interoperability does not require an Arrow library or service. The new APIs are experimental pending performance and fuzz-testing evidence.

## v1 architecture proposal (design only)

The [proposed NewtonKernels v1 architecture and gate-driven implementation roadmap](docs/v1-architecture-roadmap.md) documents a **zero-central-runtime** direction: direct C++20 APIs, independent static modules, no mandatory context/JIT/daemon/autotuner, caller-owned buffers, optional AOT hardware dispatch, columnar batches and end-to-end benchmark requirements. **This is a design proposal, not an implemented or released feature.**

## Independent performance research

The [optimization techniques and validation report](docs/optimization-techniques.md) documents general algorithms for parsing, indexing, aggregation, and graph processing, with tested improvements and limitations. All implementation source is independently authored; high-performance features remain opt-in, with no central runtime.

## Native acceleration primitives (opt-in)

In addition to the existing eight modules, independently authored headers provide AOT AVX2 multidelimiter scanning, PCLMUL quote masks, borrowed CSV decoding, checked numeric/timestamp parsers, packed exception records, exact open-addressed hash aggregation and joins, query-first hybrid posting lists, reusable bidirectional shortest paths, explicit OS memory/CPU affinity controls and caller-owned phase timing. These APIs use **no central runtime, background daemon, JIT, mandatory context or implicit threads**. Every feature is conditional on its documented input domain; new kernels have **not** been proven universally fastest. See the [native acceleration guide](docs/native-acceleration-guide.md) and [opt-in build and profiling guide](docs/build-and-profile.md).

## Optional language integrations and package tools

The [packaging and bindings guide](docs/packaging-and-bindings.md) describes the
optional in-process C ABI, Python ctypes and Rust FFI wrappers, Conan 2 local
recipe and vcpkg overlay. These are **not** published registry packages or
mandatory dependencies, and they do not add a central runtime.

## Verified experimental fused CSV benchmark

The specialized [single-pass numeric parser and aggregate](docs/columnar-and-fusion.md)
was measured against the existing parser on an equal-output, same-host
100K/1M-row fixture. It achieved **1.25× / 1.39×** in that specific runner
campaign; the earlier general fused path **lost** to the owned parser.
See [raw-sample provenance and caveats](docs/fused-pipeline-evidence-2026-11-03.md).

## Benchmarks

`cmake -S . -B build/release -DNEWTON_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release` builds opt-in microbenchmarks. Benchmark output is *not* comparable across hosts without identical datasets, toolchains, CPU governors and build flags. See the [measured strategy comparison](docs/performance-comparison.md) and [`docs/benchmarking.md`](docs/benchmarking.md).

## External comparisons (same machine, same inputs)

An independent, opt-in [competitor benchmark suite](benchmarks/competitors/README.md) compares
NewtonKernels with optimized C++ STL, Boost.Graph, and optionally Vince's CSV Parser.
Each comparison checks the full result digest, retains both winners and losses, and
exports raw JSON plus a Markdown report. Unlike internal algorithm comparisons,
these are **real external implementations** built into the comparison harness.
See the [local VM snapshot](benchmarks/results/epyc-vm-2026-11-02/REPORT.md)
for measured STL/Boost results and exact environmental limitations.
**Vince's CSV Parser has not yet been measured in this VM**; the opt-in dependency
and CI workflow are configured for an external run and must pass before publishing claims.


## External comparisons across all eight modules

**Every outcome is published, including STL-reference wins.** The latest successful GitHub Actions run measured **all 22 cases for all eight modules on the same runner**. A separate local VM campaign for five modules is retained as independent replication. **Do not compare absolute timings between hosts.**

- [Complete capabilities and all measurement parameters](docs/capability-scorecard.md)
- [All 16 newly verified five-module comparison pairs](docs/five-module-benchmarks.md)
- [Unified 22-case same-run CI results, including all wins and losses](benchmarks/results/github-actions-2026-11-02-a3ccc65/REPORT.md)
- [Previous six-case CI comparator measurements](docs/benchmark-dashboard.md)
- [Professional interactive dashboard](showcase/index.html) — select the evidence campaign explicitly
- [Reproduce the original benchmark adapters](benchmarks/competitors/README.md)

The five-module competitors are independently authored standard-library references, not full competing high-level libraries. Peak RSS, CPU time, page faults and context switches are captured as **process-wide** diagnostics; cache-miss/IPC counters, energy, NUMA and allocation tracing have not been measured.

## Performance regression protection

The [performance regression guard](docs/performance-regression-policy.md) checks all
22 historical cases for correctness and performance on each pull request. It
rebuilds the candidate, current main and the pinned published baseline on the
same runner, reports slowdowns and fails on confirmed regressions. All smaller
regressions remain review-required warnings. The GitHub `main` ruleset must
require its `compare` check before merging new kernels.

## Limitations

- `CsvParser` allocates owned `std::string` fields; for unquoted data, prefer the borrowed `for_each_delimited_record` API.
- Tokenization in `InvertedIndex` is ASCII-only; Unicode normalization, persistence and compression are not provided.
- `shortest_path` expects nonnegative integer weights and uses 64-bit distances; a non-representable candidate is skipped.
- AVX-512 requires x86 with AVX-512F/BW; all other architectures use the portable scalar path.

## Contributing & licensing

Contributions are welcome under [`CONTRIBUTING.md`](CONTRIBUTING.md). Report vulnerabilities privately as described in [`SECURITY.md`](SECURITY.md). Released under the MIT License; see [`LICENSE`](LICENSE). Only independently authorized code should be imported from other projects, retaining all required notices.
