# Architecture

The public ABI is namespaced under `newton::`. Headers are installed below `include/newton/`. Each optional functionality group exports a separately linkable static CMake target.

- `core` probes hardware/OS-supported ISA features; `simd` dispatches AVX2 byte-scanning for x86 GCC/Clang and otherwise runs portable scalar implementations.
- `memory` owns OS mapping lifetimes and aligned storage. Mapping points directly into the file: users must keep the object alive while using its views.
- `parsing` supports strict quoted CSV rows and a lightweight borrowed unquoted record iterator.
- `index` builds an in-memory positional inverted index over ASCII-alphanumeric tokens; queries use document-ID intersections.
- `analytics` contains template aggregation/join helpers and an exact nearest-rank selection routine.
- `graph` stores weighted directed adjacency in CSR form and uses a monotone radix heap for Dijkstra's algorithm.
- `parallel` executes tasks on a fixed set of threads, returning futures with exception propagation.

No module requires downloads during configuration. Performance optimizations must preserve scalar correctness, expose bounded resource use and have deterministic regression tests.
