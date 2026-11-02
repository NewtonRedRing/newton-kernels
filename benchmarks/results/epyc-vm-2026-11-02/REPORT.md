# NewtonKernels external performance comparison

> Generated from recorded, same-run executions. Lower latency is better.
> Results apply only to these workloads and environment; not a general speed claim.

## Environment

- UTC: 2026-11-02T16:38:25.415700+00:00
- CPU: AMD EPYC 9V74 80-Core Processor (5 logical CPUs)
- OS: Linux-6.18.57-x86_64-with-glibc2.41
- Compiler: c++ (Debian 14.2.0-19) 14.2.0
- Boost: 1_83
- CPU affinity: [0, 1, 2, 3, 4]
- Governor: unavailable
- Virtualization: container-other

## Results

| Workload | Size | NewtonKernels median (ms) | Competitor | Competitor median (ms) | Newton speedup | Checksum |
|---|---:|---:|---|---:|---:|---|
| scan | 1048576 | 0.0123 | stl | 0.1582 | 12.88× | `20ade35340a3edfd` |
| scan | 16777216 | 0.2847 | stl | 3.0012 | 10.54× | `c730870e6386478f` |
| graph | 5000 | 5.9316 | boost | 11.0691 | 1.87× | `c9bce3c147c8dc2d` |
| graph | 20000 | 43.7426 | boost | 103.5881 | 2.37× | `7f633daa6ec8faec` |

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
