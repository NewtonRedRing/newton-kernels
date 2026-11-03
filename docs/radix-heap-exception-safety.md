# Track 14 — radix-heap exception safety (rollback design)

The previous implementation advanced `last_` and moved the source bucket
before re-bucketing. If a destination `push_back` threw during allocation,
the heap could lose entries and become inconsistent.

The revised implementation saves the old monotone bound and keeps every
source entry in a moved local vector until **all** destination pushes succeed.
If any push throws, all entries inserted during this redistribution are
popped from their respective destinations, `last_` is restored, and the
original source vector is moved back. These rollback operations do not
allocate: integer pairs have nonthrowing destruction and vector move is
nonthrowing with the default allocator. The observable queue state and
ordering survive the exception. No extra histogram, reserve phase, or
additional allocation is introduced on the successful path.

The original two-pass reserve prototype slowed the graph benchmarks by
approximately 10–12% and was rejected; this rollback design specifically
removes that counting overhead. No speed claim is made until fresh A/B
measurements finish. A dedicated `std::bad_alloc` test verifies that all
keys remain present and ordered after an interrupted redistributing pop.

The original 22 protected cases, six CSV pipelines and platform correctness
checks remain mandatory; thresholds have not been adjusted.
