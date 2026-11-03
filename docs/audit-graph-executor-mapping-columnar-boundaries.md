# Tracks 13, 15, 16, 18 — additional correctness gates

This test-only PR adds a single standalone CTest executable to cover:

- Directed shortest paths with zero-weight edges, unreachable nodes,
  near-maximum distances, overflow rejection and repeated workspace reuse
- One-worker executor nested parallel calls, empty ranges and invalid arguments
- Empty and binary mapped files, page-boundary byte values, moves and self-moves
- Nullable column selection and aggregation with a nonzero Arrow-style validity
  bit offset, compared to an independent scalar oracle

These supplement existing randomized tests. They do not claim performance
optimization, exception-safety proof, or complete coverage of every possible
platform fault. No production source or benchmark fixtures are changed.
The original 22+6 regression guard and all multi-platform CI are retained.
