# External library comparison suite

This opt-in suite performs apples-to-apples **input and output** comparisons for three limited operations. It does **not** imply equivalent features across libraries. All adapters and fixture generators are independently implemented C++; no competitor source code is checked into this repository.

## Build

```bash
cmake -S . -B build-external -DCMAKE_BUILD_TYPE=Release \
  -DNEWTON_BUILD_BENCHMARKS=ON -DNEWTON_EXTERNAL_BENCHMARKS=ON \
  -DNEWTON_COMP_BENCH_WITH_BOOST=ON
cmake --build build-external --target newton_compare --parallel
python3 benchmarks/competitors/run.py \
  --binary build-external/benchmarks/competitors/newton_compare \
  --out build-external/comparison
```

Boost headers must be installed to run the Boost case. The runner **fails rather than fabricate a result** when a requested competitor is unavailable. Built-in STL is available without extra installation.

To benchmark [Vince's CSV Parser](https://github.com/vincentlaucsb/csv-parser), enable the separate optional dependency (version pinned by immutable commit; not vendored):

```bash
cmake -S . -B build-external -DCMAKE_BUILD_TYPE=Release \
  -DNEWTON_BUILD_BENCHMARKS=ON -DNEWTON_EXTERNAL_BENCHMARKS=ON \
  -DNEWTON_COMP_BENCH_WITH_BOOST=ON -DNEWTON_COMP_BENCH_WITH_VINCE=ON
cmake --build build-external --target newton_compare --parallel
python3 benchmarks/competitors/run.py \
  --binary build-external/benchmarks/competitors/newton_compare \
  --include-vince --out build-external/comparison
```

Network-free build: download the competitor into your own dependency cache, and pass `-DNEWTON_VINCE_SOURCE_DIR=/absolute/path/to/csv-parser`. The competitor retains its separate license. The project itself does not require either competitor.

## Workload semantics

- **Scan**: deterministic binary-safe character count on equal 1 MiB and 16 MiB buffers. Comparator: `std::count` from the compiler's standard library. Both scan identical pre-generated memory; time excludes generation.
- **Graph**: deterministic directed graph with 6 edges per node, nonnegative integer weights and 32/64 target queries. Comparator: Boost.Graph `dijkstra_shortest_paths` with early termination at the exact destination. Both preprocess graphs outside the timing. Newton reuses its epoch-tagged distance workspace; Boost must initialize its distance map each query. The report makes this tradeoff explicit.
- **CSV**: deterministic no-header comma-delimited 4-column records, with CRLF/LF, commas and newlines inside quotes, and escaped quotes. Both consume the same in-memory bytes and hash *all decoded cells*, rather than counting only rows. Vince uses `csv::parse_unsafe` with disabled threading and fixed format. This measures complete parsing/iteration/decoding without input file I/O, with known semantic restrictions; it does not compare filename-based I/O, parallel parsing or general feature coverage.

All variants are compiled in one `Release` executable for matching compiler and optimization flags, and each benchmark invocation is a fresh process. The runner alternates variant order across rounds, verifies equal output hashes + counts, and records the full timing samples. Observed data are published as JSON and Markdown. No speedup claim is made for an unexecuted competitor.

## Interpretation and caveats

- Report all cases, including cases where NewtonKernels loses. Smaller-is-better latency; reported speedup equals competitor / Newton.
- Repeat on a dedicated bare-metal server with pinned CPU affinity, fixed frequency governor, and representative production datasets before advertising general speed claims.
- The baseline is an actual competent implementation, not intentionally slow pseudocode. Different systems, graph structures, parsing semantics, and input scales can reverse the result.
- In-process RSS peaks are not incremental memory utilization, and may include shared memory or input buffers. It is reported separately from elapsed time.
- Generated `results.json` and `REPORT.md` are provenance artifacts. Do not promote them to a universal ranking.

Sources: [Boost.Graph](https://github.com/boostorg/graph) (BSL-1.0), [Vince's CSV Parser](https://github.com/vincentlaucsb/csv-parser) (MIT). Their source is never copied into `newton-kernels`.
