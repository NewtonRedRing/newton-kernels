# 20-track optimization audit — resolution register (2026-11-03)

> **Historical snapshot:** This register records the original November 3 proposals. Refer to the linked GitHub issues and merged PRs for live status; several older PR descriptions may be superseded.

Base repository: `main` at `7721f53da8ed87e6e4215c26f31b1bd5c9fa22f6`.

The 20 original rows mix **confirmed correctness risks**, **performance
hypotheses**, and **measurement gaps**. An optimization is not a fix until its
full output contract and no-confirmed-regression gates are verified. The table
below tracks every row separately; **Open** never means that the code is broken.

| Track | Problem / optimization hypothesis | Action and acceptance evidence | Status |
|---|---|---|---|
| 1 CPU dispatch | Repeated runtime initialization | Isolated #20 accelerates probe but failed the full performance guard; keep nonmerged until paired tests pass | Open, blocked #20 |
| 2 SIMD scan | Dispatch, AVX tails/downclock | Test page boundaries and scalar parity under different ISAs; benchmark 1MiB/16MiB with stable host | Open — profiling |
| 3 CSV SIMD masks | PCLMUL short-block crossover | Differential tests 0–64 bytes, initial quote parity, escaped pairs; tune threshold only on measured wins | Open — testing |
| 4 CSV parser | Quoted-field allocation | Compare zero-copy unquoted and quoted scratch paths on full grammar corpus; preserve error acceptance | Open — profiling |
| 5 CSV records | Branch-heavy state machine | Multiline/CRLF, malformed quote state, callback behavior; no untested state removal | Open — testing |
| 6 CSV numeric | Fused fast parse | Strict validation and cents-overflow equivalence with baseline; run six pipeline cases | Open — profiling |
| 7 Group map | Reserve and hash misses | Skewed vs uniform keys and allocation costs; no unproved reserve heuristic | Open — profiling |
| 8 Flat hash | Collisions and modulo | Power-of-two masking with exact-key oracle tests shipped in merged #22; more distributions still needed | Partial — #22 merged |
| 9 General hash join | Per-row node allocation | #21 shows join wins but failed the total guard; #19 combined variant also blocked | Open, blocked #19/#21 |
| 10 Fused join | Overflow and alias contract | Reject inconsistent lengths, verify duplicates and overflow; document disjoint storage requirement | Open — testing |
| 11 Positional index | Per-candidate phrase allocations; partial insertion | #23 and #24 contain isolated proposals with fault tests; both failed guard; no merge | Open, blocked #23/#24 |
| 12 Hybrid postings | Format crossover and transactional selective index | This PR fixes add_query/add_document/finalize exception rollback, with deterministic injection tests; bitmap crossover itself still needs profiling | **Fix proposed — pending CI** |
| 13 Shortest paths | Radix vs binary and overflow | Compare to independent Dijkstra oracle, unreachable/zero-weight cases, audit epoch rollover | Open — testing |
| 14 Radix heap | Rebucket memory and allocator failure | Test monotonic duplicates, empty heap, reallocation failure before adopting any reserve heuristic | Open — testing |
| 15 Executor | Reentrancy and task scheduling | Existing nested-call logic; validate shutdown, exception propagation, full drain and short-task contention | Open — profiling/testing |
| 16 Memory mapping | Page faults / I/O hints | Compare cold/warm mapped and buffered file with peak RSS; preserve OS cleanup | Open — profiling |
| 17 Aligned memory | Allocation overhead | Validate moves/zero-size and test allocator alternatives on pinned hosts | Open — profiling |
| 18 Columnar | Bitmap null path / overflow | Match independent oracle with Arrow null masks, offsets and extreme lengths | Open — testing |
| 19 C ABI | Error and alias semantics | Verify nonthrowing boundary, overflow leaves outputs untouched, Python/Rust smoke | Open — testing |
| 20 Regression guard | Unrelated-case failures, CI host noise | Do not weaken 22+6 baselines; isolate with pinned-host repetitions, IPC/cache miss/RSS measurement, separate code layout effects | Open — measurement |

## Selective index transaction fix

A failed `add_query` must not publish new term IDs without corresponding
posting storage. A failed `add_document` must not consume a document ID or
leave any partial postings. A failed `finalize` must leave raw term vectors
available for a full retry. These invariants are verified by injected
`std::bad_alloc` at successive allocation points in an isolated test
executable, and by exact AND results after retries.

**No performance claim is made.** The 22-case established benchmark,
six CSV pipeline cases, full OS builds, sanitizers and fuzz smoke all remain
merge gates. No thresholds, reference checksums or fixtures are changed.

## Status note

The original `docs/optimization-audit-20-tracks-2026-11-03.md` is the
audit snapshot. This register supplies an actionable status for each row;
its proposed fixes are not all closed. The remaining 19 tracks require
additional independent validation and, in some cases, profiler evidence.
