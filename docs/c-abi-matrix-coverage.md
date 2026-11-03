# Track 19 — run C ABI contract checks on every CI platform

Baseline: `7721f53da8ed87e6e4215c26f31b1bd5c9fa22f6`.

The optional C ABI was configured with `NEWTON_BUILD_C_API=OFF` in the
default CI matrix, so the existing `newton_c_api_tests` target did not run
on Linux, Windows, or macOS in that matrix. The CI configure step now
explicitly enables the shared C API so that the binary is built, linked,
installed, and its tests run on every platform and Debug/Release variant.

The expanded tests check output preservation on invalid pointers, null
data with a positive row count, zero-length null data, null results,
unsigned max threshold, and existing overflow behavior. The ABI and
production C++ implementation are unchanged; exported status codes and
checksums remain unchanged. Full 22-case + 6-case performance regression
guards are not relaxed.
