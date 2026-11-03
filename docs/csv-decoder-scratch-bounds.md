# CSV decoder scratch bounds and exact-capacity tests

Audit tracks 4–6 require malformed CSV and zero-copy parsing equivalence.

The quoted-field decoder previously checked scratch capacity using
`used + segment_length + 1`. On an adversarial input length near
`size_t::max`, this arithmetic could wrap before the comparison.
The candidate compares segment lengths with **remaining capacity**
using subtraction; it never needs to form the overflowing sum.

Tests cover two escaped fields with an exactly sufficient six-byte scratch,
the same row with a five-byte scratch (throws), entirely borrowed fields
with an empty scratch, too few descriptors, and unexpected bytes after
a closing quote. The accepted CSV grammar, zero-copy paths and caller
buffer ownership contract are unchanged.

Changes to code under `include/` require all 22 established performance
cases, six CSV pipeline cases, cross-platform tests, sanitizers and fuzzing
before merging. This is input-hardening, not a proven speedup.
