# Changelog

## 0.1.0 — development preview

- Initial C++20 modular API, CMake install/export targets and offline build.
- Memory mapping, aligned buffers, ASCII text search, CSV parsing, scalar/AVX2 byte scanning, aggregation, CSR graphs and thread pool.
- Unit, integration and smoke benchmarks, examples, CI, and contributor documentation.

## Unreleased — selectable performance strategies

- Independently implemented scalar / AVX2 / AVX-512 byte scans with explicit selection, runtime dispatch and differential tests.
- Added small-list-first vs input-order postings intersection; binary vs radix heap Dijkstra with reusable query state.
- Added exact percentile selection vs full sorting and deterministic strategy benchmarks.
- Published CPU-scoped comparison results and strict no-third-party-code provenance policy.
