# Columnar views, Arrow interoperability and fused execution

**Status:** v0.1 experimental public additions, subject to v1 API review.
The modules use ordinary synchronous C++20 calls, caller-owned memory and
ahead-of-time compiled code. There is no central runtime, global context,
hidden worker thread, daemon, JIT or automatic calibration.

## Borrowed data model

- `ColumnView<T>` stores a borrowed `std::span<const T>` and optional validity
  bitmap (`1 = valid`), including a non-zero bit offset.
- `StringColumnView` stores byte buffers and monotone 32-bit string offsets.
  Its constructor validates offsets once. Strings are raw bytes, not Unicode
  collations; a view must not outlive its memory.
- `RecordBatchView` is caller-owned type-erased metadata, with uniform row count.
- `select_greater_into` emits caller-owned 32-bit row IDs in row order;
  output capacity must hold the entire input row count.
- `sum_where_greater` handles nulls in both predicate and value columns and
  checks unsigned count/sum overflow.

## Apache Arrow C Data Interface

`#include <newton/columnar/arrow_c_data.hpp>` provides
`borrow_arrow_primitive<T>(array, schema)`, compatible with the standard
`ArrowArray`/`ArrowSchema` layouts, including sliced offsets and null
bitmaps. Include the producer's `<arrow/c/abi.h>` if available. NewtonKernels
does **not** depend on the Arrow C++ library.

This is a **non-owning import** of fixed-width scalar primitives only. The
caller remains responsible for invoking the producer's release callbacks
**after** the last use of the borrowed view. It rejects released arrays,
unsupported dictionaries/children, incompatible type strings, missing buffers,
and misaligned values. A producer is responsible for truthful buffer size
metadata; no C pointer API can independently prove a foreign buffer's size.

Only 32/64-bit integer and 32/64-bit floating primitive formats are currently
mapped. Strings, nested arrays, dictionary arrays, IPC, schema export,
ownership transfer, and automatic Arrow release are not implemented.

## Fused CSV to grouped aggregation

`parse_filter_group_cents_into(csv, options, counts, sums, fields, scratch)`
handles logical CSV records including escaped double quotes and newlines.
Each row requires a dense unsigned group index, unsigned filter measurement,
and nonnegative decimal value containing at most two fractional digits.
The filter is strictly greater-than. Optional header skipping is explicit.

The function borrows CSV input, uses caller-provided field descriptors and
decode scratch, produces exact integral cents and checks overflow. It does not
allocate, spawn workers or retain a row. Validation errors may leave aggregate
outputs partially updated; callers should discard them on an exception.
Unlike a general CSV-to-database pipeline, the schema and filter are intentionally
specialized. Benchmarks MUST compare equivalent parsed fields and final outputs.

### Direct use

```cpp
#include <newton/analytics/csv_fused.hpp>
std::array<std::uint64_t, 64> counts{}, sums{};
std::array<std::string_view, 4> fields{};
std::array<char, 2048> scratch{};
auto stats = newton::analytics::parse_filter_group_cents_into(
    csv_bytes,
    {.group_column=0, .filter_column=1, .value_column=2,
     .filter_greater_than=500},
    counts, sums, fields, scratch);
```

## Semantics and limitations

- All references are borrowed and require caller-managed lifetime.
- Public `*_into` operations can mutate output before an invalid-input error.
- Null rows are excluded from columnar filtering/reduction.
- The decimal parser rejects scientific notation, rounding, and negative values
  in the unsigned-only fused CSV aggregation.
- CSV output aggregates are indexed by the supplied dense group ID. Unknown
  groups are rejected rather than silently hashed or truncated.
- SIMD features elsewhere in the library remain optional and have scalar
  fallbacks; the new fused CSV path is a scalar state-machine specialization
  pending broader architecture-specific performance validation.
- The included comparison tool contrasts an allocation-conscious fused
  implementation with the existing owned-field parser under identical
  generated CSV bytes and exact outputs. It is not a universal competitive
  claim against tuned external parsers.

## Evidence and reproduction

Build `newton_pipeline_compare` with `NEWTON_BUILD_BENCHMARKS=ON`,
then use `tools/benchmark_ab.py` to alternate comparable processes with
checksum verification. Report complete end-to-end wall time, absolute memory
use and the competitor wins as well as losses.

Do not advertise measured performance unless the run's input, compiler, CPU
model, flags, raw samples and source revisions are available.
