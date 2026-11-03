# Changelog

Changes are recorded here for the development-preview series. Unreleased
features should not be interpreted as a new published release.

## Unreleased

### Documentation and developer experience

- Introduced a dedicated GitHub Pages documentation portal alongside the
  benchmark dashboard, with accessible navigation and linked source references.
- Added a central Markdown documentation index, local link validation and
  repository-hygiene ignore rules.

### Columnar, parsing and optional language integrations

- Added borrowed numeric/string column views, validity bitmaps, batch
  descriptors and Arrow C Data compatible primitive imports.
- Added caller-buffer selection/reduction, optional scalar/AVX2/ARM NEON
  filtering and checked single-pass numeric CSV aggregation.
- Added optional in-process C ABI and Python/Rust FFI wrappers, local Conan
  recipe and vcpkg overlay.
- Added focused differential testing, fuzzing, independent installed-package
  consumers and same-host benchmark evidence.

### Performance strategies and experiments

- Added explicit scalar/AVX2/AVX-512 scanning strategies with differential tests.
- Added optional postings intersection and shortest-path strategies.
- Added exact percentile selection alongside full sorting.
- Retained benchmark evidence including losses; experimental optimizations
  require the established regression checks before integration.

## 0.1.0 — development preview

- Initial C++20 modular API with offline CMake install/export targets.
- Read-only memory mapping, aligned buffers, ASCII search, CSV parsing,
  byte scanning, grouping, joins, CSR graphs and a thread pool.
- Tests, examples, CI, benchmark adapters and contribution guidance.
