# Speed Demon 2026 — Open-source research and independent technique transfer

**Status:** Curated research plus three independently authored experimental NewtonKernels primitives. This is NOT an exhaustive survey of all public entrants; official private holdout scoring and final winners were not yet published on the research date **2026-11-03**. All performance numbers from other teams are their published claims, not independently verified here.

**Competition:** https://speeddemon.dev/ — the four tracks are A log ingestion/percentiles, B search/index, C hostile CSV/join, and D graph traversal. The fixed contest hardware was Intel i5-13500, AVX2 and a 12-thread limit. **NewtonKernels must not adopt those fixed hardware, data-generator or single-run lifetime assumptions.**

## 1. Public sources examined

No contestant source was copied or vendored. We studied publicly described techniques and independently authored the implementations below.

| Repository | What was researched |
|---|---|
| [pal-123456789/speed-demon](https://github.com/pal-123456789/speed-demon) | [complete techniques](https://github.com/pal-123456789/speed-demon/blob/speed-demon/docs/TECHNIQUES.md), [parser design](https://github.com/pal-123456789/speed-demon/blob/speed-demon/docs/PARSER-TECHNIQUES.md), [hybrid postings](https://github.com/pal-123456789/speed-demon/blob/speed-demon/docs/B-POSTINGS-DESIGN.md), [CSV fast-path regressions](https://github.com/pal-123456789/speed-demon/blob/speed-demon/docs/C-FAST-PATH.md), [graph layouts](https://github.com/pal-123456789/speed-demon/blob/speed-demon/docs/D-FUSED-ROWS.md) |
| [keirsalterego/danse-macabre](https://github.com/keirsalterego/danse-macabre) | Compact intermediate representations; counting histograms; query-aware search; graph memory layout |
| [Nikhils-G/speeddemon](https://github.com/Nikhils-G/speeddemon) | [Query-first index alternative](https://github.com/Nikhils-G/speeddemon/blob/speed-demon/track-b/README.md); [CSV exactness and collision caveats](https://github.com/Nikhils-G/speeddemon/blob/speed-demon/track-c/README.md); SIMD parsing experiments |
| [codesujeet/SpeedDemon](https://github.com/codesujeet/SpeedDemon) | [Ablations and rejected optimizations](https://github.com/codesujeet/SpeedDemon/blob/speed-demon/docs/EXPERIMENTS.md); compiler, hash and phase profiling |
| [Xuefeng-Zhu/speed-demon-2026](https://github.com/Xuefeng-Zhu/speed-demon-2026) | [Experiment catalogue](https://github.com/Xuefeng-Zhu/speed-demon-2026/blob/speed-demon/experiments/README.md), quote masks, packed records, bidirectional search, graph renumbering |
| [KushnerykPavel/speeddemon_2026_solution](https://github.com/KushnerykPavel/speeddemon_2026_solution) | Parallel log parsing, CSV interning, sparse/graph experiments |
| [ikeshavvarshney/speed-is-life](https://github.com/ikeshavvarshney/speed-is-life) | [Indexing measurements](https://github.com/ikeshavvarshney/speed-is-life/blob/speed-demon/track-b/PERF.md), [join measurements](https://github.com/ikeshavvarshney/speed-is-life/blob/speed-demon/track-c/PERF.md), SIMD gather, page/huge-page tradeoffs |
| [Aryanxp1/speed-demon-2026](https://github.com/Aryanxp1/speed-demon-2026) | Multiple algorithms and toolchains across the four tracks |
| [coreyjlord-dev/speed-demon](https://github.com/coreyjlord-dev/speed-demon) | C++ implementations and individual performance logs |
| [pranaysb/speed-demon](https://github.com/pranaysb/speed-demon) | C++ entries alongside starter materials; distinguish substantive changes from baseline |
| [hackathon-raptors/speed-demon-starter](https://github.com/hackathon-raptors/speed-demon-starter) | Official reference semantics, fixtures and output checking; NOT an optimized competitor |

Other repositories are discoverable; many are forks or unmodified starter solutions. We have not measured all submissions, examined every commit, ranked winners, or established that all approaches generalize. These sources provide **hypotheses**; NewtonKernels measures its own implementations.

## 2. Extracted techniques and prioritized transfer

| Track | Algorithm / optimization idea | Generality and NewtonKernels decision |
|---|---|---|
| A | Borrowed memory-mapped scanning; avoid per-record string copies | **High** — follow with streaming byte/field views |
| A | Detect multiple delimiters in one SIMD pass | **High** — P1 ISA-specific benchmark and tiny-input scalar fallback |
| A | Specialized fixed-format timestamps / ASCII numeric parsing | **Conditional** — P1 as opt-in grammar-specific APIs; never hardcode contest dates |
| A | Integer-domain direct aggregation, fused count/sum | **High** — **implemented** as caller-owned dense arrays with checked overflow |
| A | Exact bounded-domain percentiles via histogram instead of sorting | **High when domain is small** — **implemented** for multiple quantiles with caller scratch |
| A | Pack intermediate records with rare-value escape storage | **Conditional** — P2; must preserve full numeric range |
| B | Read all queries first; index only referenced terms | **High for batch queries** — P1 opt-in mode, not a replacement for general mutable indexes |
| B | Bitmap/list hybrid postings, compact 16-bit segment IDs | **High** — P1; measure memory and mixed-list intersections |
| B | Rarest-term-first, galloping / skip intersection, query planning | **High** — P1; correctness under skew and duplicates |
| B | SIMD token-classification and hash-table gather | **Hardware-specific** — P2; retain scalar fallbacks |
| C | Quote-parity masks and CLMUL delimiter filtering | **High but tricky** — P1 AOT acceleration after independent correctness oracle |
| C | Raw logical CSV-record slicing with escaped quotes/newlines | **High** — **implemented** as allocation-free boundary scanner; NOT field decoder |
| C | Exact decimal cents, byte-exact unescaping, fused joins | **High** — P1; do not conflate quote boundary scan with full CSV parsing |
| C | Inline short keys, flat open-addressed hash, per-worker local tables | **High** — P1; guarantee exact collision resolution |
| D | Two-pass CSR and compact hot edge layouts | **High** — P1 for construction; existing CSR retained |
| D | Bidirectional Dijkstra, bucketing/radix heaps and locality ordering | **High on some graphs** — P2; include reverse-graph setup in end-to-end times |
| D | Epoch-tagged reusable workspaces | **High** — already in Newton graph; document and improve repeated queries |
| Cross | Compiler PGO/LTO, architecture-specific AOT flags | **Opt-in** — no JIT, mandatory runtime profile or global service |
| Cross | Page release, prefetch, huge pages, thread pinning | **Conditional** — P2, explicit options and full cache/OS comparison |
| Cross | Profile by phase; A/B interleaving; keep failed trials | **Required** — reproducibility and complete evidence, not selective winning anecdotes |

### Ideas explicitly rejected for a reusable library

* Hardcoding an ID range, timestamp window, fixed field count or generated corpus shape without making it part of an explicit API precondition.
* Approximate percentile calculation when exact nearest-rank results are required, or using a hash fingerprint as proof that distinct keys are equal.
* An implicit 12-thread pool, application-wide CPU pinning, direct process exit or skipped destructors.
* Unconditional page discard/prefetch/huge-page advice; these sometimes regress and can damage caller reuse patterns.
* AVX2-only builds or unsafe ISA execution on processors without relevant CPU/OS support.
* Treating a raw record-boundary scanner as equivalent to a fully decoding CSV parser.
* Copying an entrant's source, relying on third-party contest libraries, or publishing another entrant's speed measurements as NewtonKernels results.

## 3. First independent implementation transfer

The companion PR adds only direct, opt-in C++20 primitives with **no central runtime, daemon, mandatory context, JIT, hidden worker threads, automatic benchmarking or external dependencies**.

1. **Dense grouping:** [dense_group.hpp](../include/newton/analytics/dense_group.hpp) exposes dense_group_count_into and dense_group_count_sum_into. Existing caller supplies count/sum spans. O(rows + domain), no allocation by the functions. Bounds and unsigned-overflow checks; invalid inputs may leave output partly updated.
2. **Exact histograms:** [percentile.hpp](../include/newton/analytics/percentile.hpp) gains percentiles_nearest_rank_bounded_into. One histogram of a caller-declared uint64 value domain answers arbitrary quantiles, including p50/p95/p99. Caller-supplied scratch is overwritten with inclusive prefix counts. Domain check and quantile validation. Sparse/wide domains should use the existing nth_element API.
3. **CSV record boundary scanning:** [csv_records.hpp](../include/newton/parsing/csv_records.hpp) adds for_each_csv_record. Zero-allocation borrowed views; preserves raw bytes and supports CR/LF/CRLF inside quoted fields with escaped double quotes. Strict misplaced-quote validation. Does not decode field contents; a later field-decoding API is separate.

The current public APIs remain unchanged. No original participant implementation was imported.

## 4. Initial measured screening — local, NOT official contest scoring

Environment: AMD EPYC 9V74 virtual machine, five visible x86-64 CPUs; GCC 14.2 C++20 Release, repeated warm-cache measurements, median of nine. Fixed deterministic fixtures, compared results checked for equality. CPU frequency and affinity not controlled; sequential test order may bias observations. This is **developer screening**, not production or across-CPU performance evidence.

| Fixture | NewtonKernels | Alternative A | Alternative B |
|---|---:|---:|---:|
| One million records, 256 dense key values; count and uint64 sum | **0.852 ms** checked dense | **0.637 ms** manually optimized *unchecked* dense loop (**Newton loses**) | **2.066 ms** generic unordered_map (**Newton wins**) |
| 500,000 values in a 1024-value domain; exact p50/p95/p99 | **0.283 ms** bounded histogram | **9.172 ms** three independent nth_element selections | **17.328 ms** single full sort |

**Comparability limitations:** the manually optimized dense-loop alternative lacks the checked overflow contract, and the histogram exploits a *known small value domain* unavailable to a generic selection API. Do not claim universal speedups. Strong specialized competitors, whole-file I/O, memory allocations, batch cardinality/skew and ARM remain to be tested.

### Validation completed locally

* GCC 14.2 Release with warnings as errors: existing and new tests pass, **4/4 suites**.
* Clang 17 Release with warnings as errors: **4/4 suites**.
* GCC AddressSanitizer / UndefinedBehaviorSanitizer Debug: **4/4 suites**.
* Seeded differential cases: group counts/sums, exact percentiles vs existing selection, and raw CSV record boundaries versus decoded reference rows.
* Additional negative tests: invalid dense key, length mismatch, aggregate overflow, out-of-domain values, invalid quantile, malformed and unterminated CSV quoting.

Not yet verified: MSVC, ARM, ThreadSanitizer, external fuzzing, production datasets, cold-cache I/O, allocation tracing or a controlled bare-metal run.

## 5. Follow-on research backlog

**P0:** Compare histogram and grouping with equally specialized alternatives; create realistic multi-size/skew benchmarks and allocation profiles; keep every competitor win in the published dashboard.

**P1:** Design a borrowed CSV field decoder with caller scratch and optional SIMD quote masks; query-aware indexing and bitmap/posting hybrids; nonallocating grouping/join; fused parsing/aggregation. Each module gets an independent correctness oracle and a clear domain contract.

**P2:** Graph bidirectional search and compact adjacencies, optional ARM SIMD, offline PGO, explicit NUMA/parallel features. No hidden service or new mandatory dependencies.

**Experiment rule:** source concept -> independently specify algorithm -> implement -> strict differential test -> benchmark on same machine with competent alternatives -> publish all outcomes -> retain or reject. Contest-specific fast paths must not silently narrow public semantics.
