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

## Benchmarks

`cmake -S . -B build/release -DNEWTON_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release` builds opt-in microbenchmarks. Benchmark output is *not* comparable across hosts without identical datasets, toolchains, CPU governors and build flags. See the [measured strategy comparison](docs/performance-comparison.md) and [`docs/benchmarking.md`](docs/benchmarking.md).

## Verified external implementation comparisons

The reproducible [external benchmark dashboard](docs/benchmark-dashboard.md) compares the **real**
C++ STL, Boost.Graph and Vince's CSV Parser implementations on identical inputs and one
GitHub Actions machine. On the tested workloads, NewtonKernels measured **7.96–8.77×**
less time for byte scanning, **1.93–2.14×** less time for target shortest paths,
and **3.99–5.36×** less time for in-memory CSV decoding. All six comparisons
passed output-digest checks. These are **specific synthetic benchmarks, not universal
speed guarantees**. See the [raw data](benchmarks/results/github-actions-2026-11-02-5889b44/results.json),
[methodology](benchmarks/competitors/README.md), [CI evidence](https://github.com/NewtonRedRing/newton-kernels/actions/runs/41484043676),
and [independent local VM results](benchmarks/results/epyc-vm-2026-11-02/REPORT.md).

## Explore the capability showcase

**[Professional interactive showcase & complete scorecard](showcase/)** — displays all eight modules, every currently measured parameter, both competitors' figures, correctness evidence, latency, throughput, peak process RSS and unmeasured areas. The site has no cherry-picked winner filtering: competitor wins will be shown automatically.

- [Full eight-module capability matrix](docs/capability-scorecard.md) · [verified side-by-side technical comparisons](docs/benchmark-dashboard.md)
- [Reproduce the exact benchmark fixtures](benchmarks/competitors/README.md) · [machine raw results](benchmarks/results/github-actions-2026-11-02-5889b44/results.json)
- [GitHub Pages publishing guide](showcase/README.md) · [Pages deployment workflow](.github/workflows/showcase-pages.yml)

The showcased results come from a shared virtual CI runner, cover **three** of eight modules externally, and do not constitute universal superiority or library-specific RAM claims.

## Limitations

- `CsvParser` allocates owned `std::string` fields; for unquoted data, prefer the borrowed `for_each_delimited_record` API.
- Tokenization in `InvertedIndex` is ASCII-only; Unicode normalization, persistence and compression are not provided.
- `shortest_path` expects nonnegative integer weights and uses 64-bit distances; a non-representable candidate is skipped.
- AVX-512 requires x86 with AVX-512F/BW; all other architectures use the portable scalar path.

## Contributing & licensing

Contributions are welcome under [`CONTRIBUTING.md`](CONTRIBUTING.md). Report vulnerabilities privately as described in [`SECURITY.md`](SECURITY.md). Released under the MIT License; see [`LICENSE`](LICENSE). Only independently authorized code should be imported from other projects, retaining all required notices.
