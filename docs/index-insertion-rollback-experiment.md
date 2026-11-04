# Transactional in-memory index insertion: fault-injection review

Baseline: `main` commit `7721f53da8ed87e6e4215c26f31b1bd5c9fa22f6`.

## Correctness issue

Previously `add_document` incremented `documents_` before inserting the
document's postings. If an allocation failed during a term insertion or a
position vector growth, this could leave a skipped document ID and queryable
partial postings. The method provided no rollback semantics.

This change delays the document counter increment until every posting is
complete. On any exception, it removes newly inserted per-document postings
and erases empty keys introduced by the failed insertion. The rollback uses
the fully tokenized input as an existing key list; no new allocation is needed.
It preserves previous document IDs, content and positional index behavior.

## Fault-injection and review limits

A dedicated executable overrides allocation entry points for that test only;
the test injects allocation failure at each successive allocation position.
After every `std::bad_alloc`, it checks count, new term absence, untouched
existing postings/phrases, and ID reuse on successful recovery. The test also
confirms a successful insertion after the failure sweep.

The exception path is slower but is not in the successful hot loop. The
successful path adds an exception boundary and may affect binary layout;
**no speed claim is made**. Acceptance requires Linux, macOS and Windows
builds, sanitizers, fuzz smoke and paired 22+6 performance-guard success.

No relaxation of performance thresholds is permitted. A failed guard blocks
merging pending investigation, including cross-binary code-layout effects.
