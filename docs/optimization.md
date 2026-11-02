# Performance engineering principles

The library deliberately retains **multiple independently implemented, selectable strategies** that produce the same result under the documented API contract. This permits production tuning for different CPUs and input distributions rather than assuming one technique always wins.

- **SIMD byte scanning:** scalar, AVX2, AVX-512F/BW, and runtime auto-dispatch. SIMD code lives in isolated target-compiled functions; unsupported hardware safely falls back to scalar. Unaligned reads are guarded by remaining-input length.
- **Index intersection:** default rarest-postings-first (smallest candidate list first) and input-order merge. Both preserve sorted document IDs; phrase search uses an anchored positional check.
- **Graph traversal:** CSR adjacency and exact unsigned 64-bit Dijkstra with binary or monotone radix priority queues. Per-thread reusable workspaces avoid forcing global scratch state.
- **Percentiles:** nearest-rank selection (`nth_element`) or full sorting with identical rank semantics and immutable caller input.
- **Memory and I/O:** `MappedFile`, aligned storage and borrowed delimited-field views; choose owned CSV parsing for general quoted records.

Always measure before claiming speedups. Full source and reproducible synthetic comparison are in [performance-comparison.md](performance-comparison.md). Changes to architecture-specific code require scalar differential tests and sanitizer runs. Interprocess and multithread benchmarks should also inspect allocator pressure, cache misses, RSS and cold vs warm I/O. PGO and LTO remain opt-in for downstream applications.

**Provenance:** Techniques were independently reimplemented from algorithm concepts; no third-party candidate solution code was copied or imported into this library.
