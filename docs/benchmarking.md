# Benchmark methodology

Do not infer application performance from microbenchmarks alone. Configure `Release`, enable `NEWTON_BUILD_BENCHMARKS`, and pin hardware, clock policy, compiler, flags and dataset before comparing results. Run warmups, multiple trials and report median and variation; disclose CPU, RAM, operating system, NUMA/power settings, input size and correctness validation.

`newton_byte_bench` is a small deterministic byte-scanning smoke benchmark, not a validated throughput or latency comparison. Never extrapolate its output to CSV, search, joins or graphs. For each production workload, add representative, independently generated datasets and compare against a correct reference implementation. Archive raw results and repeat measurements after refactoring.
