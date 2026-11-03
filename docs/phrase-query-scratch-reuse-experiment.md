# Phrase query scratch reuse — independent experiment

Base: `main` at `7721f53da8ed87e6e4215c26f31b1bd5c9fa22f6`.

## Cost eliminated

`InvertedIndex::search_phrase` previously constructed a
`std::vector<const Posting*>` for *every candidate document*. The scratch
only needs one pointer per query term, so the candidate moves that single
allocation before the candidate loop. It retains the exact same anchor
selection, doc-id intersection, position checks, repeated-token behavior, and
result order. No caching across calls, no shared state or lock is introduced.

For an anchor list of `N` documents, this reduces temporary
pointer-vector allocations from `O(N)` to `O(1)` per query. It **does not**
guarantee a proportional end-to-end speedup: lookups and position checks
still dominate many workloads.

## Correctness

The existing index property test checks 100 randomized three-token AND and
phrase queries against an independent document-vector oracle. This PR
adds 480 additional full-result comparisons of single and repeated terms,
plus absent and empty query cases.

## Acceptance

Do not merge with any compiler/test/sanitizer/fuzz failure or a confirmed
regression in the 22 kernel + 6 pipeline cases. The existing guard does not
contain a dedicated phrase-query latency case; add that measurement before
claiming actual wall-clock improvement.

The insertion path `add_document` still increments its document count
before all posting updates succeed; allocation-failure exception safety
requires a separate guarded change and fault-injection test.
