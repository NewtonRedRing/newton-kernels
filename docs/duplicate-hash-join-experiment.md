# Duplicate-preserving hash join isolated A/B experiment

Baseline: `b33078d3adce6f8979b9b22fbdc75a3a2f040fd3`.

Switch the generic hash join from one `unordered_multimap` node per right row to `unordered_map<Key, vector<const RightItem*>>`. Preserve full key equality, every duplicate pair and existing borrowed row lifetime requirements. Per-key vectors give contiguous visitation, with a 2048-bucket maximum initial reserve hint to control hash table memory. No new dependencies or central runtime.

A small locally compiled x86-64 -O3 screening showed lower join latency on selected 3K and 12K inputs. The prior combined experiment #19 showed **15–24% faster** join on paired GitHub Actions, but also showed failures in unrelated protected benchmarks. This standalone experiment isolates the join change to investigate those failures; no timing gain is accepted until **all** regression cases pass.

Independent tests compare complete match-pair multisets against a quadratic oracle for 128 seeded input distributions, empty inputs and full string keys. Output ordering is unspecified; no output pairs are dropped or duplicated.
