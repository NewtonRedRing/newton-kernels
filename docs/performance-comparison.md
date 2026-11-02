# Reproducible performance comparison (development snapshot)

**Measured on 2026-11-02.** These are local synthetic benchmarks of *NewtonKernels C++20 implementations*, not measurements of other teams' source, independent hardware, production workloads, or contest scores. No external source code was imported.

## Environment

- Virtual machine: AMD EPYC 9V74 (5 visible virtual CPUs), Debian GNU/Linux 13, x86-64 with AVX2 and AVX-512F/BW.
- Toolchain: GCC 14.2.0, CMake 3.31.6, C++20, `Release`, normal project options (`-O3` by CMake Release).
- Timing: `std::chrono::steady_clock`, warm-up before repeated samples, median of repetitions. Table values are medians of independent benchmark-process measurements; the process itself reports a median of 7 or 9 timing trials.
- Tests: `ctest --test-dir build-perf --output-on-failure`; 3 of 3 suites passed on this host.
- Timings are sensitive to VM scheduling, CPU frequency, cache state and dataset layout. A fast synthetic kernel is not necessarily a fast end-to-end application.

## Local measured results

Lower is better. Speedup is relative to the named baseline *for this exact workload*. Strategies in a group implement the same observable result and have differential correctness tests.

| Operation and fixed input | C++ implementation | Median duration | Relative to baseline |
|---|---|---:|---:|
| Count delimiters, 256 passes over 1 MiB (256 MiB processed) | Scalar | 42.028 ms | 1.00x |
| Same count | AVX2 | 5.192 ms | 8.10x |
| Same count | AVX-512F/BW | 3.585 ms | **11.72x** |
| Same count | Runtime auto-dispatch | 3.424 ms | **12.27x** |
| 20,000 vertices, 100,000 edges, 180 source/target queries | Reusable binary-heap Dijkstra | 314.672 ms | 1.00x |
| Same graph and queries | Reusable radix-heap Dijkstra | 153.670 ms | **2.05x** |
| Same graph and queries | Fresh radix-heap workspace | 150.104 ms | 2.10x |
| 8,000 docs, 300 three-term AND queries | Input-order postings intersection | 4.811 ms | 1.00x |
| Same index and queries | Smallest-postings-first intersection | 4.288 ms | 1.12x |
| 100,000 values, 40 p95 queries | Full sort | 208.058 ms | 1.00x |
| Same values and queries | `std::nth_element` selection | 24.737 ms | **8.41x** |

**Caveats:** The two graph strategies handle nonnegative 64-bit integer edge weights, and rejected overflow candidates are treated consistently. Fresh radix happened to be slightly quicker than reuse here: avoid assuming reusable state always wins. Scan auto-dispatch chooses AVX-512 on this host and may differ from explicit AVX-512 because of timing noise. For small buffers, branch/dispatch costs may dominate.

## Run the comparison yourself

```sh
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DNEWTON_BUILD_TESTS=ON -DNEWTON_BUILD_BENCHMARKS=ON
cmake --build build-perf --parallel
ctest --test-dir build-perf --output-on-failure
./build-perf/benchmarks/newton_byte_bench
./build-perf/benchmarks/newton_strategy_bench
./build-perf/benchmarks/newton_percentile_bench
```

All benchmark inputs are deterministically generated in the corresponding `benchmarks/*.cpp`. The benchmark executables verify identical checksums where two strategies are compared. To extrapolate to your use case, first benchmark representative data at realistic scales with representative concurrency, measuring full pipeline throughput and RSS as well as isolated kernels.

## Implementation selection guidance

- **SIMD scans:** Prefer `find_byte` / `count_byte` (automatic CPU dispatch). Explicit `ByteScanStrategy` selection is mainly for benchmarking. An unsupported explicit SIMD strategy safely falls back to scalar.
- **Shortest paths:** Both radix and binary queues are exposed through `ShortestPathAlgorithm`. Benchmark the graph topology, edge-weight distribution and query types you actually use. Allocate a separate workspace per concurrent query thread.
- **Text intersection:** Smallest-postings-first benefits skewed term frequencies; input order remains available as a valid comparator.
- **Exact percentile:** Selection generally avoids sorting unneeded ranks. Full sort is retained when sorted order may be beneficial for other work; neither strategy mutates the caller's data.

## Reimplementation and provenance policy

The implementation uses generally known algorithms (Dijkstra, compressed sparse row, radix heaps, inverted indexes, percentile selection and SIMD byte comparison). Public performance writeups informed which *ideas* to evaluate; source files, constants and assembly from other competitors were not copied. Specific performance numbers from unrelated projects are not presented as NewtonKernels measurements. All changes must pass tests and independently generated edge cases before claiming equivalence.

## Independent external library comparisons

The earlier results in this document compare NewtonKernels algorithms internally.
A separately measured [external competitor comparison](../benchmarks/results/epyc-vm-2026-11-02/REPORT.md)
benchmarks the real C++ standard library (`std::count`) and Boost.Graph with
identical fixtures, an output checksum gate, fixed compiler configuration,
and alternating fresh-process runs. It is a synthetic VM screening benchmark,
**not** a universal performance ranking. The optional Vince's CSV Parser
comparison requires a separate competitor build and has not been measured
on this VM.
