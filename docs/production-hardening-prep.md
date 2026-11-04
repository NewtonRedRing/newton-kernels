# Production hardening and 33 independent issue validation streams

Status: **preparation and validation, not a release approval**. Tracks #53, #54, #55, #56, #62, and #64.

## What runs concurrently

[The 33-issue GitHub Actions matrix](../.github/workflows/issue-33-streams.yml) uses exactly one independent, read-only verification job for each still-open issue #33–#64 and #82. All 33 are eligible to run at once (max-parallel: 33), subject to GitHub's actual account concurrency and queue limits.

The first 20 jobs build and run the **existing issue-relevant C++ test executable** on Ubuntu Debug with warnings as errors. The 12 coordination jobs validate the relevant Python guard, documentation, installed consumer, or C++ boundary test. #82 checks out the immutable benchmark anchor and performs true same-executable A/A and balanced historical A/B with raw samples. Each job uploads an individual machine-readable JSON report and any CSV raw evidence **even on failure**.

These jobs do **not** constitute 33 autonomous code-writing agents. They independently verify and report; failures require source-level fixes in focused PRs. They do not replace existing eight-job platform CI, sanitizers, fuzz, complete 22-case + six CSV guard, or approved per-case high-water marks. A passing focused job cannot close an issue with unmet profiling, portability or acceptance requirements.

### Local usage

~~~sh
python3 tools/issue_streams.py --self-check
python3 tools/issue_streams.py --issue 40 --output reports/issue-40.json
~~~

Do not weaken the 3%/4-of-5 benchmark guard, remove published fixtures, or compare absolute historical milliseconds from different machines. PR #24 remains excluded from source changes under the current instruction.

## Static-analysis tooling design (#54 / #64)

| Check | Intended tool/configuration | Required evidence |
|---|---|---|
| GCC/Clang/MSVC warnings | NEWTON_WARNINGS_AS_ERRORS=ON in supported compiler matrix | Version, logs, triage, no unreviewed new warnings |
| clang-format | Pinned version, dry-run on changed C++ | Baseline record; separate formatting changes |
| clang-tidy | Pinned bugprone, performance, focused modernize and concurrency checks | compile_commands.json and disposition per finding |
| Clang Static Analyzer | scan-build against a clean build | Bug-report artifact, fix severe issues |
| Cppcheck | C++20 configuration, explicit suppression policy | XML report, severity inventory |
| GitHub CodeQL | C/C++ database and extended security queries | SARIF, severity disposition, license review |
| include-what-you-use | Opt-in analysis and independent public-header compilation | Header self-containment and documented false positives |

Tools not available on a runner must be marked **unavailable**, not passed. Baseline/configuration changes need focused PRs after #53 integration prerequisites.

## Profiler templates (#55 / #64)

For each measurable kernel retain raw cases and exact CPU/OS/compiler flags, seed, checksum and commit SHA. On supported Linux systems, gather cycles, instructions, IPC, branches/misses, cache references/misses, context switches, migrations and page faults using perf stat. Where allowed, capture perf record/flamegraphs. Document unavailable hardware counters rather than reporting zeros.

Use Intel VTune where the hardware and licensing support it; Valgrind Callgrind/Cachegrind, Heaptrack and Massif on supported configurations. Pair same-host A/B runs with rotated order, collect p50/p95/p99 and throughput, CPU time, RSS, and include negative results. Separate cold start, steady-state, memory-bandwidth and thread contention.

High-water promotions are separate reviewed PRs after a final-head pass and merge; never pin unmerged branches or docs-only noise.

## ABI, package, security and release checklist (#56 / #64)

- GCC and Clang on Linux, MSVC on Windows, Apple Clang on macOS; Debug/Release, install package and independent out-of-tree consumer in supported configurations.
- ASan, UBSan, bounded fuzz with corpus, seed and timeout, TSAN where supported, and an explicit register of unavailable tools.
- Standalone public-header compilation, C ABI symbol visibility, null and aliasing contracts, exception-to-status mapping and Python/Rust FFI lifetime tests.
- Reproducible commands, artifact hashes, license/dependency inventory, CMake exports and installed-package correctness, API/ABI compatibility and versioning.
- CSV parsing boundary behavior, arithmetic overflow, resource ownership, thread shutdown and fault-injection rollback with minimum reproducers.
- Preserve original 22 benchmark and six CSV pipeline cases, checksums, published anchor and approved high-water baselines.
- Require final merged commit SHA and complete CI/run references, security triage and explicit residual risks before release approval.

## Closing criteria

A targeted lane may pass while its issue remains open. Close a module audit only when all original acceptance items have reproducible evidence and the validated fix is merged; close the P0 integration parent only after overlaps (#19/#20/#24/#25 and successors) are reconciled. This document satisfies **design preparation only**, not a finished release audit.

[Full 33-issue register](open-issue-resolution-2026-11-04.md).
