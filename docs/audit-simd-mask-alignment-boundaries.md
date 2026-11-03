# Audit tracks 2, 3, 17 — correctness boundary matrix

The existing SIMD strategy tests covered selected fixed offsets, and the
CSV structure-mask suite covered standalone blocks of length 0–64. The new
differential tests cover every length from 0 through 257, seven misalignment
offsets, embedded NUL and high bytes, all four runtime-dispatch modes and an
independent scalar find/count oracle. For CSV structural masks, randomized
multi-block inputs carry quote parity across block boundaries (including a
forced doubled quote split at byte 63/64). An aligned-buffer test verifies
zero-size allocation and move assignment ownership.

These are **correctness gates**, not performance optimizations or proof of
every possible input. Hardware-specific CPU downclock, cache misses, and
real file/page-edge behavior still need representative profiling and memory
protection tests. No production source, benchmark fixture or slowdown
threshold is changed.
