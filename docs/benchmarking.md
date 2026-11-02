# Benchmark methodology

See [the reproducible local comparison table](performance-comparison.md), including CPU, input sizes, timings, speedups and limitations.

Benchmark with Release builds (`-DNEWTON_BUILD_BENCHMARKS=ON`) and keep compiler, flags, machine, power policy, NUMA topology, dataset size, worker count and correctness requirements fixed. Run warmups and multiple repetitions; record both typical latency and variation. Avoid mixing measurements from different hosts or silently adopting published third-party measurements as if they were replicated.

Microbenchmarks are only screening evidence; representative end-to-end datasets must also include parsing, allocations, I/O, aggregation, output generation and peak memory. Change a single performance variable at a time, use control reruns, and retain alternate strategies only where they are correct and useful.
