# Opt-in compiler optimization and rigorous performance measurement

Normal builds keep all optimization instrumentation **off**. Hardware-specific code is ahead-of-time compiled and runtime-dispatched only by local function calls. There is no daemon, JIT, background calibration, mandatory global context or hidden thread pool.

## Link-time optimization

```sh
cmake -S . -B build-ipo -DCMAKE_BUILD_TYPE=Release -DNEWTON_ENABLE_IPO=ON
cmake --build build-ipo --parallel
ctest --test-dir build-ipo --output-on-failure
```

## Profile-guided optimization (GCC, opt-in)

Train on representative full-sized datasets; do not profile only tiny synthetic samples. Use separate build trees and retain profile provenance:

```sh
mkdir -p profiles/gcc
cmake -S . -B build-profile -DCMAKE_BUILD_TYPE=Release \
  -DNEWTON_PGO_MODE=GENERATE -DNEWTON_PGO_DATA="$PWD/profiles/gcc" -DNEWTON_BUILD_BENCHMARKS=ON
cmake --build build-profile --parallel
# Run representative applications / benchmark suites built with instrumentation.
# Make sure they exercise the same functions the optimized build will use.
cmake -S . -B build-pgo -DCMAKE_BUILD_TYPE=Release \
  -DNEWTON_PGO_MODE=USE -DNEWTON_PGO_DATA="$PWD/profiles/gcc"
cmake --build build-pgo --parallel
```

For Clang, specify a .profraw filename for GENERATE, merge using llvm-profdata, then pass a .profdata file for USE. Toolchain/PGO versions must match. PGO/IPO can *reduce* speed and increase build time; keep an unprofiled control and test every build independently. MSVC PGO wrappers are not implemented.

## Independent A/B experiments

`tools/benchmark_ab.py` runs **ABBA / BAAB** repeated process-level comparisons using identical input arguments. Each implementation prints one JSON object with the same checksum/digest and item count; the runner aborts on disagreement and emits complete raw timing samples. It measures wall time inclusive of startup, not isolated CPU cycles. Example:

```sh
python3 tools/benchmark_ab.py --before './build/before --size 1000000' \
  --after './build/after --size 1000000' --cycles 16 --output build/results/ab.json
```

`newton::core::ScopedPhase` allows explicit, caller-owned per-phase timings. Its clock reads cost time and must be disabled in production hot paths. For hardware counter profiling, use supported OS performance tools separately, with correct permissions; do not fake unavailable cache/energy measurements.

## Reproducibility

Control compiler/flags, CPU affinity and frequency, input bytes, random seeds, cache state, thread count, allocation behavior and library revisions. Publish both positive and negative outcomes, with confidence intervals and complete samples. Never compare absolute runtime across different machines to claim one implementation faster. An improvement is not accepted if correctness changes or a realistic end-to-end case regresses without an acknowledged tradeoff.
