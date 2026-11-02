# NewtonKernels: five-module external-reference benchmarks

> Every result is reported, including cases where the C++ standard-library baseline is faster.
> Paired implementations were tested in alternating fresh processes with identical fixture seeds.

## Environment

- UTC: 2026-11-02T17:16:53.578899+00:00
- Host: AMD EPYC 9V74 80-Core Processor / 4 logical processors
- OS: Linux-7.0.0-1017-azure-x86_64-with-glibc2.39
- Compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
- Affinity: [0, 1, 2, 3]
- Governor: None
- Virtualization: microsoft

## All results

| Module / operation | Size | Newton (ms) | STL reference (ms) | Newton ratio | Faster | Equal digest |
|---|---:|---:|---:|---:|---|---|
| core_features | 2,000 | 0.00663 | 0.00337 | 0.51× | STL baseline | Yes |
| core_features | 200,000 | 0.65807 | 0.32496 | 0.49× | STL baseline | Yes |
| memory_align | 1,024 | 0.01241 | 0.02067 | 1.67× | Newton | Yes |
| memory_align | 131,072 | 0.34982 | 0.62490 | 1.79× | Newton | Yes |
| memory_map | 1,048,576 | 1.18312 | 1.19979 | 1.01× | Within 2% | Yes |
| memory_map | 16,777,216 | 18.37626 | 19.19106 | 1.04× | Newton | Yes |
| index_query | 4,000 | 0.06299 | 0.11456 | 1.82× | Newton | Yes |
| index_query | 12,000 | 0.57347 | 0.70752 | 1.23× | Newton | Yes |
| index_build | 1,000 | 1.25106 | 1.26395 | 1.01× | Within 2% | Yes |
| index_build | 4,000 | 4.88235 | 4.91163 | 1.01× | Within 2% | Yes |
| analytics_group | 20,000 | 0.04889 | 0.05758 | 1.18× | Newton | Yes |
| analytics_group | 200,000 | 0.48607 | 0.48890 | 1.01× | Within 2% | Yes |
| analytics_join | 3,000 | 0.10093 | 0.09304 | 0.92× | STL baseline | Yes |
| analytics_join | 12,000 | 0.58526 | 0.48222 | 0.82× | STL baseline | Yes |
| parallel_tasks | 50 | 0.38628 | 1.75065 | 4.53× | Newton | Yes |
| parallel_tasks | 20,000 | 2.63959 | 3.54793 | 1.34× | Newton | Yes |

## Complete supported resource evidence

| Operation | Size | Newton RSS peak (KiB) | STL RSS peak (KiB) | Newton min–max (ms) | STL min–max (ms) |
|---|---:|---:|---:|---|---|
| core_features | 2,000 | 19244 | 19244 | 0.0066–0.0243 | 0.0033–0.0035 |
| core_features | 200,000 | 19244 | 19244 | 0.6497–0.6716 | 0.3249–0.3742 |
| memory_align | 1,024 | 19244 | 19244 | 0.0123–0.0125 | 0.0205–0.0209 |
| memory_align | 131,072 | 19244 | 19244 | 0.3422–0.3731 | 0.6095–0.6442 |
| memory_map | 1,048,576 | 19244 | 19244 | 1.1735–1.2021 | 1.1911–1.2584 |
| memory_map | 16,777,216 | 20368 | 20308 | 18.3417–18.5206 | 19.0619–19.5639 |
| index_query | 4,000 | 19244 | 19244 | 0.0525–0.1186 | 0.0638–0.1569 |
| index_query | 12,000 | 28696 | 28696 | 0.5627–0.6311 | 0.6936–0.7367 |
| index_build | 1,000 | 19244 | 19244 | 1.2312–1.3185 | 1.2414–1.2997 |
| index_build | 4,000 | 19244 | 19244 | 4.8095–4.9482 | 4.8641–5.0348 |
| analytics_group | 20,000 | 19244 | 19244 | 0.0475–0.0530 | 0.0461–0.0797 |
| analytics_group | 200,000 | 19244 | 19244 | 0.4698–0.5156 | 0.4698–0.5231 |
| analytics_join | 3,000 | 19244 | 19244 | 0.0779–0.1484 | 0.0744–0.1268 |
| analytics_join | 12,000 | 19244 | 19244 | 0.5673–0.6762 | 0.4023–0.5585 |
| parallel_tasks | 50 | 19244 | 19244 | 0.3196–0.6611 | 1.6326–1.8568 |
| parallel_tasks | 20,000 | 19244 | 19244 | 2.0893–2.7184 | 2.8930–5.3727 |

## Operational definitions and limitations

- **core_features:** CPU capability discovery vs identical compiler builtins; x86 GCC/Clang only. No actual vectorized computation.
- **memory_align:** Both allocate 64-byte aligned buffers, initialize and sample identical elements; includes allocation and deallocation.
- **memory_map:** Read-only mmap vs 64KiB buffered std::ifstream, including open, read and cleanup. Input file generated off-clock; warm OS page cache.
- **index_query:** Same ASCII positional inverted index semantics, 2-term AND, full result digest; both build indexes off-clock.
- **index_build:** Both build positional indexes and answer 2-term AND queries within the timer; baseline uses STL postings with term positions.
- **analytics_group:** Group-count API vs a reserved and manually optimized std::unordered_map; sorting/digest included for both.
- **analytics_join:** Multimap hash join vs unordered_map-of-vectors hash join with exact duplicate multiplicity; unordered checksum.
- **parallel_tasks:** Four persistent Executor workers vs std::async(std::launch::async) in bounded groups of four; identical task computation.

**Important:** 'STL reference' means an original C++ baseline built from standard facilities; it is not a third-party library benchmark or a market ranking.
Peak RSS and CPU resource data include runtime and fixture preparation, not incremental allocation attributed to each library.
Process-wide resource counters are available in `results.json`; hardware cache/IPC counters, energy and isolated allocation counts are **not measured**.
The timings are synthetic warm-cache benchmarks, not production throughput, full feature equivalence or cold-start tests.
Build-source code is original. No competitor source was imported. Run `run_extended.py` again on your machine and compare pairs within the same environment.
