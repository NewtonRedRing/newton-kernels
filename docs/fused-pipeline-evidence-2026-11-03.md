# Same-host columnar/fused CSV evidence — 2026-11-03

> Scope: Correctness-checked generated data, **not** an external competitor ranking or universal speed claim.
> Source run: [GitHub Actions 41678424947](https://github.com/NewtonRedRing/newton-kernels/actions/runs/41678424947).
> Tested branch revision: `22b4981de60c1515d25bf3db036bd05661ea9fa0`.

## Method

All implementations read **identical pre-generated CSV files** using the
project's memory-mapped I/O. Each process includes startup, mapping,
parsing, aggregation and final result materialization, but not fixture
generation. The same one-run machine was used with 12 ABBA/BAAB cycles,
two warm-ups, and full raw timing samples retained as a workflow artifact.
Checksums, row counts and selected counts matched exactly.

Input fixture descriptions: deterministic 100,000 and 1,000,000 row files
of group ID, integer filter field and 2-decimal currency field; every ninth
group ID is quoted. Group cardinality 64, filter threshold 500. The
benchmark compares NewtonKernels' existing `CsvParser` plus exact
dense aggregation against two specialized paths.

## Results (median process wall time)

| Rows | Existing owned parser | Single-pass numeric parser | Relative speedup | Earlier generic fused parser |
|---:|---:|---:|---:|---:|
| 100,000 | 5.755 ms | 4.586 ms | **1.255×** | 6.188 ms (slower) |
| 1,000,000 | 45.164 ms | 32.412 ms | **1.393×** | 48.217 ms (slower) |

Input/output comparisons: 100K produced 49,900 selected records and
checksum `4709875033890892831`; 1M produced 499,000 selected and
checksum `17466304306262528293`.

The faster path has a **narrow numeric CSV grammar**. It accepts quoted
numeric tokens and validates logical CSV field boundaries, but does not
materialize arbitrary strings or provide a general decoded schema. The
general parser remains necessary for full CSV features.

The alternative generic fused parser, despite avoiding some allocations,
was slower because it still scanned records and decoded fields separately.
This negative result is retained deliberately.

## Limitations

This is a GitHub-hosted Linux runner, not pinned bare-metal hardware.
System load, cache residency, CPU frequency and affinity were not fully
controlled. No independent measurement of hardware counters, allocations,
peak RSS, cold-cache performance or multiple CPU generations is included
in these ratios. These values must not be extrapolated to other datasets.

The raw sample JSON files and fixture SHA256s are attached to the run.
The CI success is an **output/correctness/workflow claim**, not a guarantee
of faster execution in all applications.

## Required future work

- Repeat at scale with multiple real-world datasets, varying quote rates,
  null/invalid values and parsing complexity.
- Benchmark against strong external parsers under identical semantics.
- Pin dedicated x86 and ARM hardware; capture memory behavior and perf counters.
- Gate performance regressions by workload size without discarding losses.
