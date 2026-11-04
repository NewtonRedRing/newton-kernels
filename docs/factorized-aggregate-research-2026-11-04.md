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
