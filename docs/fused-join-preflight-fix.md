# Track 10 — fused join preflight safety

Baseline: `7721f53da8ed87e6e4215c26f31b1bd5c9fa22f6`.

`fused_join_group_sum_into` previously reset the caller-owned output arrays
before `flat_hash_join_into` validated bucket and link scratch capacities.
A failed precondition could therefore erase valid results without performing
any join. This patch checks both scratch capacities **before** resetting the
outputs. The existing numeric overflow behavior (partial output on a
runtime arithmetic failure) is unchanged; the API's non-aliasing requirement
is unchanged.

New tests cover empty heads, truncated link arrays, invalid group keys,
unchanged outputs on invalid inputs, and arithmetic overflow with duplicate
matches. No benchmark fixture, threshold, or output checksum is changed.
The performance guard and cross-platform CI remain merge blockers.
