# Track 20 — fail-closed regression evidence validation

Baseline: `7721f53da8ed87e6e4215c26f31b1bd5c9fa22f6`.

The 22-case performance guard checked only the **presence** of three output
signature fields, and accepted Python booleans as numeric times. An upstream
benchmark output schema error could therefore be mistaken for valid evidence.
The core decision also accepted nonfinite or zero timing values if called
outside the normal `measure()` function.

This change validates the actual classic/extended benchmark output types:
`checksum` is a nonempty hexadecimal string fitting a 64-bit value,
`items` and `input_bytes` are nonnegative integers (not booleans), and
timings are finite positive real numbers (not booleans). The paired
comparison rejects invalid values before division. The pipeline guard's
helper similarly rejects nonfinite and nonnumeric deltas.

Policy unit tests inject malformed checksums, counts, sizes, and timings,
then verify normal examples still pass. **The original 22 kernel cases and
6 CSV cases, 3% policy threshold, rounds, repetitions, and historical
anchor remain unchanged**. No “noise” failure waiver is introduced. This
hardens validation but does **not** solve hardware/VM variance; pinned-host
repeats and hardware counters remain open tasks.
