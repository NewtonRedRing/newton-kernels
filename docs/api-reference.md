# API reference (v0.1)

Public declarations and contracts live in `include/newton/` and are the canonical API reference. Modules:

- `memory::MappedFile(path)` opens and maps a read-only file, exposes `bytes()`, `text()`, `size()`. An empty file is valid. Move-only.
- `memory::AlignedBuffer<T, Alignment>(count)` creates aligned uninitialized trivial storage. Move-only.
- `simd::find_byte(input, needle)` returns offset or `input.size()`; `count_byte` counts exact byte matches.
- `parsing::CsvParser::parse(input, callback)` returns row count and invokes a callback with decoded string fields. Throws for malformed quotes.
- `parsing::for_each_delimited_record(input, delimiter, callback)` yields borrowed string-view fields for unquoted records.
- `index::InvertedIndex::add_document(text)` assigns IDs; `search_all(text)` and `search_phrase(text)` return matching IDs.
- `analytics::group_count(rows, projection)` and `hash_inner_join(left, right, keys, visitor)` are generic in-memory templates.
- `analytics::percentile_nearest_rank(values, q)` returns an exact nearest-rank order statistic, `q` in `[0, 1]`.
- `graph::GraphBuilder(n)` / `CsrGraph` define a directed weighted sparse graph; `shortest_path(graph, from, to)` returns optional distance.
- `parallel::Executor(n)` supports `submit` and `parallel_for` with task exception propagation.

No stability guarantees are made until v1.0.
