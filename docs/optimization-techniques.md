# Native optimization techniques and validation

This document records independently implemented algorithm concepts and evidence for NewtonKernels. It is not a universal performance ranking. Each mechanism is opt-in, portable implementations remain available, and no central runtime is required.

## Implemented and evaluated

- Dense integer-key counting and checked sum with caller-provided output buffers.
- Exact bounded-domain nearest-rank percentiles using a caller-provided histogram.
- Allocation-free logical CSV record-boundary scanning, preserving raw quotes and embedded newlines.

The original controlled screening on an AMD EPYC virtual machine used a 1M-row, 256-key group count/sum fixture. The checked dense aggregation took 0.852 ms, an unchecked dense loop 0.637 ms (faster), and a general `std::unordered_map` reference 2.066 ms (slower). For 500K values in a 1024-value domain, exact p50/p95/p99 took 0.283 ms with the histogram, 9.172 ms with three independently computed selections, and 17.328 ms with a full sort. These are *workload-specific* and neither input domains nor checking semantics were equivalent in every comparison. Source data and raw samples from other machines must not be used as NewtonKernels results.

## General engineering directions

| Subsystem | Techniques and prerequisites |
|---|---|
| Parsing | Borrowed views, multi-delimiter SIMD scans, quote-aware CSV boundaries, strict field decoding, checked ASCII numerics |
| Analytics | Direct-address groups, bounded histograms, exact integer arithmetic, flat collision-resolving tables, fused filter/group paths |
| Index | Query-first selective materialization, compact postings, bitmap/list hybrids, rarest-first and galloping intersection |
| Graph | Two-pass CSR construction, bidirectional Dijkstra when reverse adjacency is available, epoch-tagged scratch, compact graph layouts |
| Hardware | Ahead-of-time ISA variants, explicit thread ownership, opt-in OS memory hints and compiler PGO/LTO |
| Methodology | Independent correctness references, representative datasets, phase profiling, alternating A/B trials and retaining losing results |

## Restrictions

Do not assume a fixed dataset, date range, processor, field count, graph topology or input grammar without explicit public preconditions. Never use hash identity as proof of string equality. Never skip destructor work, forcibly terminate the calling process, or spawn hidden threads. Report performance wins and losses with corresponding correctness and resource evidence.
