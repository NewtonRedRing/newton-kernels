# Source audit: correctness and exception-safety fixes (2026-11-03)

## Reviewed scope
Audited the 191-entry repository tree at main `e1250bc6bb50ba5547a16f8b45579d59a0288131` (including 135+ tracked blobs), with targeted close reading of the C++20 library implementation and public headers in core, memory, SIMD, parsing, index, analytics, graph, parallel and columnar, plus the optional C ABI, Python/Rust adapters, tests and CI configuration. Documentation, large historical benchmark JSON and UI assets were inventoried; this is **not** a formal proof that every line is defect-free.

## Concrete findings fixed
1. **Executor reentrancy / deadlock**: `parallel_for()` called by a task running on the same single-worker pool previously enqueued futures and waited for itself, deadlocking. It now executes its nested partitions inline on that pool's worker, with per-thread ownership detection. Other pools may still schedule normally.
2. **Executor exception safety**: `parallel_for()` enqueued tasks capturing the caller's functor by reference before allocating all future slots. If vector growth or submission failed, outstanding tasks could outlive the functor. Reserve before launching tasks and drain submitted futures if later submission fails.
3. **Bitmap sizing on 32-bit**: the `universe + 63` expression could wrap at `UINT32_MAX`, erroneously choosing a zero-length bitmap and then performing out-of-bounds writes. Use overflow-safe quotient/remainder rounding and density decision.
4. **C ABI output alias**: `newton_sum_gt_u64` silently overwrote one result when both outputs pointed to the same object. Return `NEWTON_STATUS_INVALID_ARGUMENT` without changing either output.

## Verification requirements
- Run all CTest suites under Linux/Windows/macOS Debug+Release and sanitizers; optional C ABI mode must run `newton_c_api_tests` as well. Run existing libFuzzer smoke target.
- Run all **22 historical benchmark cases** and the **six established CSV pipeline cases** with same-host before/after verification; preserve any negative timing evidence. This branch must **not** bypass a failing or missing regression check.
- Runtime checks include reentrant single-worker completion, callback exception propagation and task drainage, C ABI output alias rejection/overflow atomicity, and full uint32 posting-domain construction.
- Prefer new guard PR #4 (if merged) before merging this change. As of branch creation, the protection PR and docs PR were open and `main` was not verifiably protected.

## Unresolved coverage and design risks
- No complete proof for arbitrary pointer provenance/alias in borrowed spans, externally supplied Arrow buffer capacities, or C ABI raw pointers; these interfaces necessarily rely on documented caller-owned lifetimes.
- The normal CTest matrix builds without NEWTON_BUILD_C_API, so optional ABI correctness requires a separate CI build to exercise the new test.
- The upstream `Executor` API remains an unbounded queue. Self-destruction from a worker thread is unsupported. The changes only fix reentrant `parallel_for`, not arbitrary user-written blocking `submit().get()`.
- Stress/fault-injection tests for exception paths and long-running performance stability remain advisable.
