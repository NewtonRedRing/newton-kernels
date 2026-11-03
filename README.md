# NewtonKernels

[![CI](https://github.com/NewtonRedRing/newton-kernels/actions/workflows/ci.yml/badge.svg)](https://github.com/NewtonRedRing/newton-kernels/actions/workflows/ci.yml)
[![Performance regression guard](https://github.com/NewtonRedRing/newton-kernels/actions/workflows/performance-guard.yml/badge.svg)](https://github.com/NewtonRedRing/newton-kernels/actions/workflows/performance-guard.yml)

**Portable, dependency-light C++20 kernels for data processing, indexing and graph analytics.**

NewtonKernels supplies independently linkable static libraries, public headers,
correctness tests and reproducible benchmark evidence. Implementations can use
supported hardware instructions with portable fallbacks. No third-party runtime
dependency is mandatory.

> **v0.1 development preview.** Public APIs can change before v1.0.
> This is not a distributed runtime, GPU framework or SQL engine.
> Measured speedups apply only to the exact cited workloads.

**Start here:** [Documentation](showcase/docs/index.html) ·
[Build and install](showcase/docs/getting-started.html) ·
[API modules](showcase/docs/modules.html) ·
[Architecture](showcase/docs/architecture.html) ·
[Benchmark dashboard](showcase/index.html) ·
[Raw evidence](benchmarks/results/github-actions-2026-11-02-a3ccc65/REPORT.md)

## Build and test

Requires CMake 3.21+ and a C++20 compiler (GCC, Clang, or MSVC).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNEWTON_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For multi-config generators, use `--config Release` and `ctest -C Release`.
To build and install a package, see the [installation guide](docs/getting-started.md)
and [package consumption examples](tests/consumer/).

## Core modules

| CMake target | Main capability |
|---|---|
| `NewtonKernels::core` | CPU feature detection |
| `NewtonKernels::memory` | Mapped files and aligned buffers |
| `NewtonKernels::simd` | Byte scanning and portable/ISA-specific strategies |
| `NewtonKernels::parsing` | Quoted CSV and borrowed delimited record parsing |
| `NewtonKernels::index` | Positional inverted index and phrase queries |
| `NewtonKernels::analytics` | Exact grouping, joins and percentile selection |
| `NewtonKernels::graph` | Weighted CSR graphs and shortest paths |
| `NewtonKernels::parallel` | Explicit bounded worker executor |
| `NewtonKernels::columnar` | Opt-in header-only borrowed columnar views |

The [API guide](showcase/docs/modules.html) describes ownership and lifetime
requirements. Additional experimental native primitives, C ABI, Python/Rust
bindings, Conan and vcpkg support are documented in the
[technical index](docs/README.md).

## Benchmark evidence

The committed primary campaign contains 22 same-run comparison cases across
eight modules; a separate local VM campaign contains 16 cases. Both winners
and losses are published, with full raw samples and input checksums.

- [Interactive comparison dashboard](showcase/index.html)
- [Performance methodology](showcase/docs/benchmarks.html)
- [Competitor harness and commands](benchmarks/competitors/README.md)
- [Capability and measurement scorecard](docs/capability-scorecard.md)
- [22-case primary raw samples](benchmarks/results/github-actions-2026-11-02-a3ccc65/results.json)
- [16-case separate VM samples](benchmarks/results/local-extended-2026-11-02/results.json)
- [Mandatory regression policy](docs/performance-regression-policy.md)

Do **not** combine absolute results from different machines. Whole-process
peak RSS does not isolate library allocations. No universal fastest claim
is made.

## GitHub Pages and documentation maintenance

The entire dependency-free static site lives under `showcase/`, including
`showcase/docs/`. It is published by
[the Pages workflow](.github/workflows/showcase-pages.yml), once the repository
owner has selected GitHub Actions as the Pages source.

```sh
python3 tools/build_showcase.py --check
python3 tools/check_docs.py
python3 -m http.server 8000 --directory showcase
```

Browse `http://localhost:8000/docs/` locally. Detailed technical research,
dated experiment notes and future roadmaps remain available via
[docs/README.md](docs/README.md); they are historical evidence, not generated
junk to be deleted.

## Contribute and support

Read [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md) and
[CHANGELOG.md](CHANGELOG.md). New kernels need correctness checks and the
existing unmodified regression gate. Vulnerabilities should be reported
privately through the repository's security channel where available.

License: [MIT](LICENSE).
