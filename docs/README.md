# NewtonKernels documentation index

This is the canonical map of project documents. Start with the
[professional documentation portal](../showcase/docs/index.html)
or its [GitHub Pages build](https://newtonredring.github.io/newton-kernels/docs/)
(only when deployment has succeeded). Public headers under
[`include/newton/`](../include/newton/) are authoritative for signatures.

## Start here — current development preview

- [Getting started](getting-started.md) — toolchains, installation and use.
- [API reference](api-reference.md) — module contracts and public interfaces.
- [Architecture](architecture.md) — dependency boundaries and ownership.
- [Columnar and fused kernels](columnar-and-fusion.md) — experimental views and adapters.
- [Packaging and bindings](packaging-and-bindings.md) — CMake, C, Python, Rust, Conan and vcpkg.
- [Native acceleration guide](native-acceleration-guide.md) — opt-in advanced primitives.

## Measurement and reproduction

- [Benchmarking](benchmarking.md) — methodology and fair comparisons.
- [Capability scorecard](capability-scorecard.md) — measured/unmeasured scope.
- [Performance comparison](performance-comparison.md) — baseline results and caveats.
- [Performance regression policy](performance-regression-policy.md) — unchanged protected gates.
- [Build and profile](build-and-profile.md) — optional IPO/PGO and local A/B.
- [Benchmark dashboard](benchmark-dashboard.md) — **historical** six-case report.
- [Five-module benchmarks](five-module-benchmarks.md) — separate-VM evidence.
- [Fused pipeline evidence](fused-pipeline-evidence-2026-11-03.md) — date-scoped experiment.

## Issue resolution and evidence

- [2026-11-04 open-issue resolution register](open-issue-resolution-2026-11-04.md)
  — all 33 open GitHub issues, verified progress, blockers and objective
  criteria required before closure. The GitHub issues remain the live tracker.

## Engineering history and audits (retain for provenance)

These are dated investigations, point-in-time experiment notes, or proposals.
They are **not** the current product specification and should not be removed
solely because their associated pull request was merged, changed or superseded.

- [20-track audit overview](optimization-audit-20-tracks-2026-11-03.md)
- [Source audit](source-audit-2026-11-03.md)
- [SIMD, mask and alignment](audit-simd-mask-alignment-boundaries.md)
- [Graph, executor, mapping and columnar](audit-graph-executor-mapping-columnar-boundaries.md)
- [C ABI matrix coverage](c-abi-matrix-coverage.md)
- [CSV scratch bounds](csv-decoder-scratch-bounds.md)
- [Duplicate-preserving hash join](duplicate-hash-join-experiment.md)
- [Fused join preflight](fused-join-preflight-fix.md)
- [Phrase scratch reuse](phrase-query-scratch-reuse-experiment.md)
- [Radix heap exception safety](radix-heap-exception-safety.md)
- [Performance guard evidence validation](performance-guard-validation-fix.md)
- [Native optimization techniques](optimization-techniques.md)
- [Optimization principles](optimization.md)

## Proposals and roadmaps (not shipping commitments)

- [v1 architecture roadmap](v1-architecture-roadmap.md)
- [Enterprise-kernel implementation plan](enterprise-kernel-implementation-plan.md)

## Repository hygiene

- Do not commit `build/`, profiler scratch, `__pycache__`, editor swap files,
  logs, local virtual environments or machine-specific output.
- Keep committed benchmark evidence under `benchmarks/results/` and the
  reproducible site copies under `showcase/data/` intact.
- Use `python3 tools/check_docs.py` and
  `python3 tools/build_showcase.py --check` before changing documentation.
- Make link repairs and content updates in reviewable PRs; audit reports should
  be amended or explicitly superseded rather than silently destroyed.
