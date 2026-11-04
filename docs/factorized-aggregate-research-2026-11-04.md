# Research: factorized dense-key join aggregation (2026-11-04)

**Experimental and opt-in. Not an established general competitor speedup.**

## Mechanism

The existing `fused_join_group_sum_into` enumerates every matching left/right pair. If a query needs only per-group **counts and sums**, exact algebra eliminates pair enumeration:

- Count the number `f(k)` of right rows for each bounded integer key `k`.
- Each left row with key `k`, group `g` and value `v` adds `f(k)` to the group count and `v × f(k)` to the group sum.
- The total pair count likewise increases by `f(k)`.

The result is identical to visiting all pairs when values are exact nonnegative uint64 integers and checked accumulations do not overflow. Overflow is explicitly detected instead of wrapped. **This method cannot produce pair-level output** and is not a replacement for the ordinary full join. Right-key domain validation is done before any output mutation. Left keys outside the bounded domain have no matches, consistent with an equijoin.

Complexity is `O(domain + left_count + right_count)` versus `O(left + right + pair_matches)` for a materialized/visited join. The method uses caller-owned `O(domain)` histogram scratch and does no heap allocations inside the call. Full general-key joins remain available.

This independent implementation follows factorized aggregation as an algorithmic idea. Relevant published research includes the [2025 DuckDB adaptive factorization paper](https://duckdb.org/library/factorization/) and [2026 SplitJoin paper](https://www.duckdb.org/library/splitjoin/).

## Correctness gates

- `tests/unit/test_factorized_dense_join.cpp` evaluates 1,200 randomized cases against a full quadratic duplicate-pair oracle, plus empty, duplicate, out-of-domain, `UINT64_MAX`, invalid scratch and integer-overflow cases.
- GCC C++20 with ASan+UBSan passed in a local isolated build; Clang 17 Release passed.
- The branch adds the test to normal cross-platform CI. **No protected benchmark fixture or threshold was changed.**

## Preliminary independent local screen (not the new CI target)

Initial screening used an Intel Xeon Platinum 8573C container, a pinned logical CPU, GCC `-O3`, 32K left / 16K right rows, 16 output groups, full checksum parity, 17 alternating rounds, and three repeated program runs. Both variants reused scratch and allocated equivalent output vectors. The reference was the **unmerged CSR pair-enumerating join from PR #75**, which visits every actual pair; the factorized algorithm computes counts without visiting pairs. This is an algorithm comparison on a specialized aggregate task, **not** a comparison to an independently factorized best-in-class competitor.

| Key domain | Exact matching pairs | Observed paired-run local speedup range vs CSR enumeration |
|---:|---:|---:|
| 64 | ~8.00 million | 268×–292× |
| 256 | ~2.00 million | 50×–76× |
| 4,096 | ~125 thousand | 3.6×–5.9× |

These are exploratory medians without interval estimates, dedicated bare-metal replication, p99, energy, production datasets or independent library comparisons. They must not be marketed as universal product speedups. The branch's `newton_factorized_dense_join_bench` instead compares against the **existing mainline fused hash-join implementation** with exact group outputs and equal preallocated scratch lifecycle.

## Tradeoffs and next experiments

- Dense-key scratch is expensive for a huge or sparse key range. Future work should compare a cache-friendly sparse frequency table and radix-partitioned histograms, selecting only when cost models justify it.
- If joined pairs are required individually, this algorithm is inapplicable; use the existing full join.
- For independent competitive evaluation, compare against a capable factorized SQL aggregate (e.g. DuckDB), not just an enumerating hash join, on the same exact query semantics, data and hardware.
- Test all-left-unmatched, extremely heavy skew, zero-valued rows, overflow, cold vs warm cache, allocations, scratch RSS, cycles, throughput, p95/p99 and different compilers.
- Rerun all existing 22 kernel and six CSV performance checks on the **final PR head**. Preserve the current mandatory 3% guard and high-water locks.

## New exact aggregate-only competitive screening (2026-11-04)

A **different** comparison now avoids the pair-enumeration-vs-factorization
mismatch. The [GCC/Clang and DuckDB benchmark job](https://github.com/NewtonRedRing/newton-kernels/actions/runs/41817486575)
ran successfully on GitHub Actions. Raw CSV/JSON, exact checksums, p50/p95/p99
samples, DuckDB SQL plans and process-wide memory evidence are uploaded as
workflow artifacts.

### Specialized C++ versus two independently implemented factorized alternatives

Every implementation computes the same duplicate-preserving grouped count/sum
from 32K left and 16K right rows. Each run includes right-side index
construction and aggregation, excludes fixture generation, and uses 63
interleaved repetitions per input. The hashed competitor is an
explicitly reserved `std::unordered_map<key,count>` frequency table.
The third path sorts right keys and uses `equal_range`, also without
enumerating pair output. These are independent algorithmic alternatives, not
different implementations of the same cache layout.

| Key width | GCC dense p50 ms | GCC reserved hash p50 ms | Ratio hash/dense | Dense scratch | Hash allocation bytes |
|---:|---:|---:|---:|---:|---:|
| 64 | .048012 | .084066 | 1.75× | 512 B | 2,072 B |
| 256 | .048152 | .084597 | 1.76× | 2 KiB | 8,200 B |
| 4,096 | .055773 | .179258 | 3.21× | 32 KiB | 131,752 B |
| 65,536 | .100770 | .815609 | 8.09× | 512 KiB | 473,336 B |
| 262,144 | .087972 | .867287 | 9.86× | 2 MiB | 504,656 B |
| 1,048,576 | .174071 | .892725 | 5.13× | 8 MiB | 512,288 B |

Clang runs also favor the dense algorithm on these cases but ratios are
compiler-dependent. The sorted alternative was measured and is retained as
a potential sparse/immutable-input strategy, not omitted because it lost.
Tracked allocations use a counting allocator for these benchmark strategies;
whole-process peak RSS includes all fixtures and runtime.

The 1,048,576-key case demonstrates the *memory* disadvantage of dense
histograms despite its median CPU-time win. Adaptive dispatch and a scratch
budget are necessary to avoid overclaiming wins in every resource dimension.

### External DuckDB 2.0.0 SQL on the exact same generated rows

The DuckDB job built the same deterministic left/right records from integer
formulas and checked exact group counts/sums across four strategies:
- Newton's C++ bounded-key histogram aggregate;
- DuckDB 2.0.0 explicit factorized SQL with `GROUP BY` on the right keys;
- DuckDB 2.0.0 normal `JOIN` followed by grouped aggregation;
- DuckDB explicit factorization using four threads.

It uses 39 randomized process-level experimental blocks. Input preparation
is excluded from timing in all four paths. A single-core affinity is used.

| Key width | Newton p50 | DuckDB factorized SQL 1 thread p50 | Ratio |
|---:|---:|---:|---:|
| 64 | 0.089 ms | 2.711 ms | 30.5× |
| 4,096 | 0.089 ms | 3.789 ms | 42.6× |
| 262,144 | 0.166 ms | 3.842 ms | 23.1× |

**Not general-purpose product superiority:** Newton is a narrowly specialized
C++ function. DuckDB performs SQL planning/execution, group-materialization and
Python API invocation, so the invocations and features are unequal. This is a
real exact-output workload comparison but not evidence of universal speedup
against a database engine. The resource measurements are whole-process, not
library-attributed. Report all slower cases from the artifacts, not just
these representative rows.

### Protected guard status

[Performance regression guard run 41817486407](https://github.com/NewtonRedRing/newton-kernels/actions/runs/41817486407)
FAILED the two historical CSV cases: candidate vs previous main was -0.69%
and +0.03%, but relative to the old pinned anchor was +8.27% and +8.87%.
All cross-platform CI and optional competitor jobs passed, but **PR #81 must
remain draft/unmerged**. Do not weaken the guard or waive these unrelated
failures. [Issue #82](https://github.com/NewtonRedRing/newton-kernels/issues/82)
tracks exact A/A and binary-layout investigation.
