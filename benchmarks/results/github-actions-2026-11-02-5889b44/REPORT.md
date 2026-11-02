# NewtonKernels external performance comparison

> Generated from recorded, same-run executions. Lower latency is better.
> Results apply only to these workloads and environment; not a general speed claim.

## Environment

- UTC: 2026-11-02T16:42:50.314225+00:00
- CPU: AMD EPYC 7763 64-Core Processor (4 logical CPUs)
- OS: Linux-7.0.0-1017-azure-x86_64-with-glibc2.39
- Compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
- Boost: 1_83
- CPU affinity: [0, 1, 2, 3]
- Governor: unavailable
- Virtualization: microsoft

## Results

| Workload | Size | NewtonKernels median (ms) | Competitor | Competitor median (ms) | Newton speedup | Checksum |
|---|---:|---:|---|---:|---:|---|
| scan | 1048576 | 0.0232 | stl | 0.1845 | 7.96× | `20ade35340a3edfd` |
| scan | 16777216 | 0.3401 | stl | 2.9813 | 8.77× | `c730870e6386478f` |
| graph | 5000 | 6.7109 | boost | 12.9449 | 1.93× | `c9bce3c147c8dc2d` |
| graph | 20000 | 53.7169 | boost | 115.1625 | 2.14× | `7f633daa6ec8faec` |
| csv | 10000 | 0.7303 | vince | 3.9126 | 5.36× | `d02505a05a299e94` |
| csv | 100000 | 7.3581 | vince | 29.3661 | 3.99× | `f5e7be38609c9fca` |

## Interpretation

Speedup = competitor median / NewtonKernels median; values below 1× mean NewtonKernels is slower.
The median is calculated across alternating fresh processes, each reporting a median of in-process repetitions.
Each pair was checked for equal digest and item count on the exact same generated fixture.
Memory is captured per process as peak RSS in the raw JSON; it includes input and runtime overhead.
Graph measures target queries with reusable Newton workspace vs Boost's standard distance-map initialization.
CSV compares in-memory processing of normalized cells; 1 thread, no header, fixed comma delimiter; both include their own parser overhead.
Scan compares `newton::simd::count_byte` with optimized `std::count`, counting the same delimiter.
Input construction, graph build, output serialization, and input storage are excluded from timed sections.
The benchmark is a synthetic screening workload and does not prove superior end-to-end application performance.

## Reproduction

See the repository `benchmarks/competitors/README.md` for the exact commands and fairness limitations.
Raw results: `results.json`. Keep this file together with the raw data.
