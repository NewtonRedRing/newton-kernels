# Native acceleration portfolio — opt-in direct C++20 APIs

**Status:** experimental independently authored algorithms. No central runtime,
daemon, JIT, mandatory global execution context, implicit thread pool or
background profiling. Core modules remain individually linkable static libraries.
New headers are installed by the existing CMake install rules.

These are not universal replacements for stdlib algorithms or established
numerical libraries. Each API has a domain of validity and costs which the
caller must include in end-to-end benchmarking.

## Module map and contracts

| Primitive / header | Operation, ownership and prerequisites | What is not promised |
|---|---|---|
| \`simd/multi_scan.hpp\` | \`find_any3\`: first occurrence of any 3 bytes, AVX2 AOT + scalar; safe for short inputs | Always faster than \`find_first_of\` |
| \`simd/token_lookup.hpp\` | \`ascii_token_mask32\`: first 32 ASCII alnum bits; \`gather4_u64\`: bounds-checked four slot lookup | Hash-key equivalence, full Unicode classification |
| \`parsing/structural_mask.hpp\` | \`csv_structure_mask64\`: comma/CR/LF mask outside quote parity, optional AVX2/PCLMUL; carry \`inside_quotes\` across blocks | Complete CSV validity or field decoding |
| \`parsing/csv_records.hpp\` | Existing borrowed logical-record scanner, handles embedded quoted newlines and escaped quotes | Field decoding |
| \`parsing/csv_fields.hpp\` | \`decode_csv_fields_into\`: caller-provided field descriptors + escape buffer; views borrow input or scratch | Lifetime beyond borrowed buffers, whole-file CSV parser |
| \`parsing/ascii_numeric.hpp\` | Checked \`parse_ascii_uint64\`, exact \`parse_decimal_cents\` and strict UTC timestamp with grammar \`YYYY-MM-DDTHH:MM:SSZ\` | Floats, time zones, leap seconds, localized formats |
| \`memory/packed_records.hpp\` | \`pack_uint64_into\` + \`unpack_uint64_at\`, 32-bit codes with sorted exact u64 exceptions; caller owns arrays | Compression win when many values exceed u32 |
| \`memory/hints.hpp\` | Explicit, optional \`advise_bytes\` for Linux mapping hints; rejects unsafe partial-page \`DontNeed\` | OS support or always-positive performance effects |
| \`analytics/dense_group.hpp\` | Existing dense count/checked count+sum with caller-provided arrays | Sparse or unbounded keys |
| \`analytics/percentile.hpp\` | Existing exact bounded nearest-rank histogram with caller scratch | Sparse or huge value domains |
| \`analytics/flat_hash.hpp\` | \`FlatCountSum\`: open addressed exact u64 keys; \`flat_hash_join_into\`: caller-provided buckets/links, duplicate-preserving matches | Best performance for every cardinality or skew |
| \`analytics/inline_key_counter.hpp\` | \`InlineKeyCounter\`: exact bytewise string keys, up to 24 bytes inline; arbitrary-long owned fallback; \`merge_from\` for caller-owned tables | Zero allocations for long strings, Unicode collation |
| \`analytics/fused_join.hpp\` | \`fused_join_group_sum_into\`: join with duplicates plus dense group count/sum; no intermediate pair array | Global sorting/order guarantee |
| \`index/hybrid_postings.hpp\` | \`HybridPostings\`: bitmap or u16 segmented offsets, exact membership; \`SelectiveTermIndex\`: query-first Boolean AND; \`intersect_galloping_into\`: sorted ID intersection | Phrase search (use existing \`InvertedIndex\`), Unicode normalization |
| \`graph/bidirectional.hpp\` | \`make_reverse_graph\`, \`bfs_reorder_graph\`, and caller-owned \`BidirectionalShortestPathWorkspace\`; exact nonnegative-weight queries | Faster one-shot queries after reverse-graph construction overhead |
| \`parallel/affinity.hpp\` | Explicit \`pin_current_thread\` on supported platforms, \`prefetch_read\` hint | Global affinity management, implicit threads |
| \`core/phase_trace.hpp\` | Explicit caller-owned \`ScopedPhase\` timings | Zero cost if instrumented |

### Semantics

- String comparisons, query terms and CSV byte operations use explicitly documented ASCII/byte semantics. Hash fingerprints **never** substitute for full-key equality.
- Joined row pairs retain duplicates. Join output order is unspecified; checksum or sort before comparing alternative hash-table arrangements.
- Every \`*_into\` API documents caller capacity and may throw on invalid capacity, overflow or malformed input; partial mutation on error is possible. Scratch cannot alias input unless expressly documented.
- Borrowed field views become invalid when caller-owned input/scratch is released or mutated. No hidden caches retain views.
- Histogram percentile requires a known bounded \`uint64_t\` value domain; inverse length-domain cost can exceed sorting for wide/sparse values.
- Query-first selective indexing is immutable after \`finalize\` and only supports Boolean AND on known ASCII-alphanumeric terms. The old position-aware index remains available for phrase semantics.
- Bidirectional traversal expects the supplied reverse graph to exactly transpose forward edges and preserve weights; it uses separate explicit workspace for each concurrent query.
- OS advice and architecture-specific instructions are **optional**. Scalar and unsupported-platform fallback logic is retained.

## Hardware and compilation

- Scalar baseline works on GCC, Clang and MSVC. Runtime capability checks are local, cached, and not part of an autonomous service.
- AOT vector variants are enabled on GCC/Clang x86 for AVX2 and AVX2+PCLMUL. ARM and MSVC use portable scalar implementations for these new primitives until dedicated variants are implemented.
- \`NEWTON_ENABLE_IPO=ON\` enables an opt-in link-time optimization check.
- \`NEWTON_PGO_MODE=GENERATE|USE\` and \`NEWTON_PGO_DATA\` select offline GCC/Clang instrumentation profiles; normal builds never read profiling data or run autotuning. See [build-and-profile.md](build-and-profile.md).
- Standard \`newton::parallel::Executor\` threads are constructed **only when the application explicitly creates an executor**. No kernel in this portfolio creates threads.

## Build, test, reproduce

\`\`\`sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNEWTON_BUILD_TESTS=ON -DNEWTON_BUILD_BENCHMARKS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/benchmarks/newton_native_primitives_bench
\`\`\`

\`newton_portfolio_tests\` verifies scalar/ISA equivalence, exact parsing,
out-of-range/overflow checks, packed-record round trips, multi-key joins,
randomized graph distances, and selective query correctness.

For unbiased whole-process comparisons, see
[build-and-profile.md](build-and-profile.md) and \`tools/benchmark_ab.py\`.

## Current evidence boundary

The original 22-case repository benchmark suite predates this portfolio.
Its timings **do not** certify these new APIs. The standalone primitive
screening benchmark is a development convenience, not a universal win claim.
Before promoting any new kernel, run many realistic inputs with equivalent
semantics and strong baselines, trace peak memory and allocations, and include
cold/warm I/O and setup costs. Publish negative and tied outcomes.

## Further hardware-specific research

Candidate optimizations not yet proven across machines include ARM NEON or SVE
specializations, direct vectorized quote-to-field decoding, fused SIMD
tokenization with corpus indexing, NUMA-aware graph partitioning, topology
aware dynamic AOT thresholds, hardware counter IPC/cache studies, and full
production-size data-pipeline comparisons. Keep these out of headline claims.
