# NewtonKernels

[![CI](https://github.com/NewtonRedRing/newton-kernels/actions/workflows/ci.yml/badge.svg)](https://github.com/NewtonRedRing/newton-kernels/actions/workflows/ci.yml)

**Portable, dependency-light C++20 kernels for data processing, indexing, and graph analytics.**

NewtonKernels provides independently linkable static libraries with stable, documented C++ interfaces and focused correctness tests. Implementations use safe scalar fallbacks and optional runtime-dispatched AVX2 on supported x86 processors. There are no mandatory third-party runtime dependencies.

> **Status: v0.1.0 development preview.** APIs can change before v1.0. Correctness, tests, packaging and portability are prioritized over unsupported performance claims. This is not yet a distributed, GPU or MPI computing framework.

## Modules

| CMake target | APIs | Notes |
|---|---|---|
| `NewtonKernels::core` | `cpu_features()` | CPU features available to current process |
| `NewtonKernels::memory` | `MappedFile`, `AlignedBuffer` | Read-only mapped I/O, aligned storage |
| `NewtonKernels::simd` | `find_byte()`, `count_byte()` | AVX2 on compatible x86 with scalar fallback |
| `NewtonKernels::parsing` | `CsvParser`, `for_each_delimited_record()` | Quoted CSV or zero-copy unquoted lines |
| `NewtonKernels::index` | `InvertedIndex` | In-memory ASCII token / phrase search |
| `NewtonKernels::analytics` | `group_count()`, `hash_inner_join()`, `percentile_nearest_rank()` | Aggregation and exact selection |
| `NewtonKernels::graph` | `GraphBuilder`, `CsrGraph`, `RadixHeap`, `shortest_path()` | Sparse weighted directed graphs |
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

`cmake -S . -B build/release -DNEWTON_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release` builds opt-in microbenchmarks. Benchmark output is *not* comparable across hosts without identical datasets, toolchains, CPU governors and build flags. See [`docs/benchmarking.md`](docs/benchmarking.md).

## Limitations

- `CsvParser` allocates owned `std::string` fields; for unquoted data, prefer the borrowed `for_each_delimited_record` API.
- Tokenization in `InvertedIndex` is ASCII-only; Unicode normalization, persistence and compression are not provided.
- `shortest_path` expects nonnegative integer weights and uses 64-bit distances; a non-representable candidate is skipped.
- Features beyond available scalar and AVX2 dispatch are not claimed or benchmarked.

## Contributing & licensing

Contributions are welcome under [`CONTRIBUTING.md`](CONTRIBUTING.md). Report vulnerabilities privately as described in [`SECURITY.md`](SECURITY.md). Released under the MIT License; see [`LICENSE`](LICENSE). Only independently authorized code should be imported from other projects, retaining all required notices.
