# NewtonKernels: five-module external-reference benchmarks

> Every result is reported, including cases where the C++ standard-library baseline is faster.
> Paired implementations were tested in alternating fresh processes with identical fixture seeds.

## Environment

- UTC: 2026-11-02T17:07:09.596725+00:00
- Host: AMD EPYC 9V74 80-Core Processor / 5 logical processors
- OS: Linux-6.18.57-x86_64-with-glibc2.41
- Compiler: c++ (Debian 14.2.0-19) 14.2.0
- Affinity: [0, 1, 2, 3, 4]
- Governor: None
- Virtualization: container-other

## All results

| Module / operation | Size | Newton (ms) | STL reference (ms) | Newton ratio | Faster | Equal digest |
|---|---:|---:|---:|---:|---|---|
| core_features | 2,000 | 0.00708 | 0.00382 | 0.54× | STL baseline | Yes |
| core_features | 200,000 | 0.71740 | 0.39194 | 0.55× | STL baseline | Yes |
| memory_align | 1,024 | 0.01204 | 0.01213 | 1.01× | Within 2% | Yes |
| memory_align | 131,072 | 0.40833 | 0.34363 | 0.84× | STL baseline | Yes |
| memory_map | 1,048,576 | 1.42637 | 1.41509 | 0.99× | Within 2% | Yes |
| memory_map | 16,777,216 | 20.12739 | 20.42480 | 1.01× | Within 2% | Yes |
| index_query | 4,000 | 0.07853 | 0.10189 | 1.30× | Newton | Yes |
| index_query | 12,000 | 0.61649 | 0.75952 | 1.23× | Newton | Yes |
| index_build | 1,000 | 1.33034 | 1.28051 | 0.96× | STL baseline | Yes |
| index_build | 4,000 | 4.86476 | 4.73125 | 0.97× | STL baseline | Yes |
| analytics_group | 20,000 | 0.04803 | 0.04735 | 0.99× | Within 2% | Yes |
| analytics_group | 200,000 | 0.53890 | 0.48471 | 0.90× | STL baseline | Yes |
| analytics_join | 3,000 | 0.11071 | 0.09536 | 0.86× | STL baseline | Yes |
| analytics_join | 12,000 | 0.67483 | 0.55339 | 0.82× | STL baseline | Yes |
| parallel_tasks | 50 | 0.33371 | 1.91064 | 5.73× | Newton | Yes |
| parallel_tasks | 20,000 | 1.74060 | 4.01866 | 2.31× | Newton | Yes |

## Complete supported resource evidence

| Operation | Size | Newton RSS peak (KiB) | STL RSS peak (KiB) | Newton min–max (ms) | STL min–max (ms) |
|---|---:|---:|---:|---|---|
| core_features | 2,000 | 96400 | 96400 | 0.0071–0.1032 | 0.0038–0.0039 |
| core_features | 200,000 | 96400 | 96400 | 0.7038–1.1085 | 0.3790–0.5010 |
| memory_align | 1,024 | 96400 | 96400 | 0.0118–0.0125 | 0.0117–0.0648 |
| memory_align | 131,072 | 96400 | 96400 | 0.3274–0.6837 | 0.3272–0.7128 |
| memory_map | 1,048,576 | 96400 | 96400 | 1.1753–1.5615 | 1.1949–2.1518 |
| memory_map | 16,777,216 | 96400 | 96400 | 19.6421–23.7200 | 19.9708–21.6129 |
| index_query | 4,000 | 96400 | 96400 | 0.0612–0.2214 | 0.0651–0.1413 |
| index_query | 12,000 | 96400 | 96400 | 0.5818–0.8081 | 0.7002–0.9911 |
| index_build | 1,000 | 96400 | 96400 | 1.1722–1.7040 | 1.1662–2.4779 |
| index_build | 4,000 | 96400 | 96400 | 4.5840–5.9720 | 4.6264–8.4533 |
| analytics_group | 20,000 | 96400 | 96400 | 0.0467–0.0805 | 0.0464–0.1129 |
| analytics_group | 200,000 | 96400 | 96400 | 0.4787–0.8196 | 0.4671–0.8102 |
| analytics_join | 3,000 | 96528 | 96528 | 0.0784–0.3270 | 0.0728–0.2421 |
| analytics_join | 12,000 | 96528 | 96528 | 0.5581–1.2414 | 0.3984–0.9018 |
| parallel_tasks | 50 | 96528 | 96528 | 0.2136–0.5605 | 1.1218–9.3098 |
| parallel_tasks | 20,000 | 96528 | 96528 | 1.5910–2.6393 | 2.1436–6.6279 |

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
