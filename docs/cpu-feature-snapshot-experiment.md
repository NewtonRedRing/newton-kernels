# CPU detection isolated experiment — 2026-11-03

Base main: `b33078d3adce6f8979b9b22fbdc75a3a2f040fd3`.

Initialize the compiler's process-usable instruction flags at most once using a C++ thread-safe local static, then return a copy. This avoids repeated `__builtin_cpu_init()` in hot paths. The feature snapshot is immutable and remains the same on portable non-x86 fallback platforms.

The combined CPU+join experiment on PR #19 improved the two feature-probe cases by approximately 49–55%, but the combined run had reproducible failures in unrelated memory/aggregation cases. This **isolated** PR makes it possible to detect whether a CPU-only change is safe; its numbers and all warnings must be reviewed independently.

No relaxation of correctness, benchmark fixtures or regression thresholds is permitted. Merge only if required Linux, macOS, Windows, sanitizers, fuzz, external comparison and `regression-guard` checks succeed without a confirmed slowdown.


## Current isolation

`cpu_features()` retains the original runtime detection path so existing callers do not inherit an unproven deployment-wide change. The new `cpu_features_cached()` API is opt-in and snapshots the process-usable flags using thread-safe static initialization. Equality with default detection and compiler flags is tested. No default-path speedup is claimed. Both the full performance guard and cross-platform CI remain required before merging.
