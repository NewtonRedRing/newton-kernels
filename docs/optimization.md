# Optimization principles

Prefer evidence-led changes: define input distributions and correctness contracts, profile before altering algorithms, minimize allocations and cache misses, and validate with independently generated edge cases. The byte scanning module uses runtime detection to avoid invoking unsupported AVX2 instructions and retains a scalar implementation.

PGO and LTO are integration-level decisions and are intentionally not forced on downstream builds. Unsafe pointer access or architecture-specific intrinsics should be isolated, bounded, tested and compiled only for supported targets. Always rerun unit tests, sanitizers and end-to-end benchmarks after optimizing.
