# Track 14 — radix-heap strong exception safety

On a pop that had to redistribute a nonempty radix bucket, the original
algorithm updated `last_` and moved the entire bucket before pushing its
entries into new buckets. If a vector allocation then failed, some entries
were no longer reachable. A later pop could silently lose them.

The candidate computes the new bucket occupancy first, reserves the required
capacity without mutating keys or the monotone bound, and only then moves
entries and updates `last_`. A failed pre-allocation may increase capacity,
but the observable heap contents, order and size remain unchanged. Entry
moves are nonthrowing (pairs of integers) and all destination capacity is
pre-reserved. The successful path does extra counting; any performance
slowdown requires an honest no-regression evaluation.

A dedicated `std::bad_alloc` injection test checks all keys and the
monotonic order after each failed redistribution attempt, then verifies
a successful pop. The 22+6 performance guard and all multi-platform
correctness checks remain mandatory.
