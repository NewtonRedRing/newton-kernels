# Performance regression policy (mandatory before new kernels)

**Purpose:** prevent silent, repeatable slowdowns in the already-published NewtonKernels benchmark portfolio. This protection must be active before accepting new performance changes.

## Protected evidence and comparisons

- The historical, published **22-case** benchmark suite is pinned to commit
  [a3ccc650](https://github.com/NewtonRedRing/newton-kernels/commit/a3ccc65021181561aec1c8fe284bdeb7c8e1234c)
  (2026-11-02). The immutable suite manifest lives in
  `tools/performance_guard.py`: six classic cases and 16 extended cases,
  protecting **all eight existing modules**.
- Every pull request to `main` builds the candidate, the pull request's current
  base revision, and the published anchor **on the same GitHub Actions runner**
  with the same compiler and CMake options.
- Every push to `main` also checks the new revision against its prior commit
  and the published anchor.
- Each original case executes five interleaved rounds of 11 in-process benchmark
  repetitions, using fixed fixture parameters and the original seed.
- The six existing CSV pipeline cases (owned, fused, fast; 100K and 1M rows)
  are separately protected against **previous main** using shared
  baseline-generated fixtures and seven ABBA/BAAB rounds each. The fixed
  November 2 anchor predates these cases and cannot evaluate them.
- The branch ruleset must select the unique **`regression-guard`** check,
  not generic `compare` (used in other workflows).
- Exact output checksum, item count, fixture size, CLI parameters, and process
  determinism are verified across all three builds. Missing cases, binaries,
  invalid JSON, build failures, or disagreements fail the job.
- Any positive paired-median slowdown is surfaced as a warning; the check
  **fails** when the candidate is >3% slower and slower in at least four of five
  paired rounds against **either** reference. If Git confirms production/build/benchmark
  trees are byte-identical to the previous main, differences are treated as
  VM scheduling warnings rather than proof of a code-caused regression. All raw samples and the report
  are uploaded, including warnings and losses.

**Interpretation:** A GitHub-hosted VM cannot guarantee zero variance, and a
finite benchmark suite cannot prove that all inputs are faster. The 3% blocking
threshold is a measurement-noise guardrail, **not permission to accept known
regressions**. A maintainer should investigate every warning before merging;
retest borderline cases on dedicated/pinned hardware, including p99, memory,
and workloads newly affected by changes. No speed claim should be published
without representative end-to-end evidence.

## Make this check genuinely required

At the moment this workflow was introduced, GitHub reported `main` as
**unprotected**. A workflow alone does not prevent bypass through direct pushes,
admins, or disabling checks.

As repository administrator, go to **Settings → Rules → Rulesets → New branch
ruleset**, target `main`, and enable:

1. Require a pull request before merging and block direct pushes.
2. Require status checks to pass, selecting the check named
   **`regression-guard`** from workflow **Performance regression guard**.
   Also require the existing CI build/test check(s).
3. Require branches to be up to date before merging; disallow bypass except
   explicit emergency administrators and require evidence for any exception.
4. Restrict or review edits to `.github/workflows/performance-guard.yml`,
   `tools/performance_guard.py` and the fixed benchmark fixtures. Consider
   CODEOWNERS/approval rules so a PR cannot silently remove the guard.

The exact check name is `regression-guard`; select it from a completed Actions run in the
GitHub UI. The GitHub connector could not configure repository branch
protection automatically.

## Adding new benchmarks

For every new public kernel, add a reference implementation and stable,
representative fixtures. Add new cases to the protected suite **without
removing, renaming, shrinking, or changing any of the original 22 cases**.
Store reproducible source hashes and measure setup, allocations, tail latency,
memory, and complete application performance. Change blocking thresholds
only through a separately reviewed policy PR. Avoid comparing historical
absolute timings across different physical machines.

## Run locally

Build `newton_compare` and `newton_compare_extended` for each of the
three revisions using the CMake switches in the workflow, then run:

```sh
python3 candidate/tools/performance_guard.py \
  --candidate-classic build-candidate/benchmarks/competitors/newton_compare \
  --candidate-extended build-candidate/benchmarks/competitors/newton_compare_extended \
  --base-classic build-base/benchmarks/competitors/newton_compare \
  --base-extended build-base/benchmarks/competitors/newton_compare_extended \
  --anchor-classic build-anchor/benchmarks/competitors/newton_compare \
  --anchor-extended build-anchor/benchmarks/competitors/newton_compare_extended \
  --candidate-sha <candidate> --base-sha <base> --out performance-evidence
```

The workflow checks that source trees under CMake/configuration, src, include and benchmarks match the previous main, and suppresses noise-only blocking if they are identical. Every reported measurement is still preserved.

The new CSV pipeline gate uses only current-main vs candidate measured on the same runner and never compares to historical milliseconds on different hosts.

The original 22-case guard never uses previously recorded milliseconds as a blocking comparison;
the older published results were measured on a different host. Instead, it
rebuilds the published **source commit** and remeasures it alongside the
candidate on the same machine.
