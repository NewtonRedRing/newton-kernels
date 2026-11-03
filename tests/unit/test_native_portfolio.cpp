#include <newton/analytics/flat_hash.hpp>
#include <newton/analytics/fused_join.hpp>
#include <newton/analytics/inline_key_counter.hpp>
#include <newton/core/phase_trace.hpp>
#include <newton/graph/bidirectional.hpp>
#include <newton/graph/shortest_path.hpp>
#include <newton/index/hybrid_postings.hpp>
#include <newton/memory/packed_records.hpp>
#include <newton/memory/hints.hpp>
#include <newton/parallel/affinity.hpp>
#include <newton/parsing/ascii_numeric.hpp>
#include <newton/parsing/csv_fields.hpp>
#include <newton/parsing/structural_mask.hpp>
#include <newton/simd/multi_scan.hpp>
#include <newton/simd/token_lookup.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>
namespace {
void check(bool value, const char* error) {
    if (!value) throw std::runtime_error(error);
}
template<class E, class F>
void throws(F&& f) {
    bool yes = false;
    try { f(); } catch (const E&) { yes = true; }
    check(yes, "Expected exception was not thrown");
}
void scan_test() {
    std::mt19937 rng(31);
    for (std::size_t n = 0; n < 220; ++n) {
        std::string text(n, 'x');
        for (auto& c : text) c = static_cast<char>(rng() & 255);
        const std::array<char, 3> needles{',','\n',' '};
        const auto found = std::find_if(text.begin(), text.end(), [&](char x) {
            return x == needles[0] || x == needles[1] || x == needles[2];
        });
        check(newton::simd::find_any3(text, needles[0], needles[1], needles[2]) ==
              static_cast<std::size_t>(found - text.begin()), "Three-byte SIMD scan mismatch");
        const auto mask = newton::simd::ascii_token_mask32(text);
        std::uint32_t expected = 0;
        for (std::size_t i = 0; i < std::min<std::size_t>(n, 32); ++i) {
            const auto c = static_cast<unsigned char>(text[i]);
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9')) expected |= std::uint32_t{1} << i;
        }
        check(mask == expected, "ASCII token classification mismatch");
    }
    const std::array<std::uint64_t, 5> table{7, 9, 0, 44, 99};
    check(newton::simd::gather4_u64(table, {4,1,0,4}) ==
          (std::array<std::uint64_t, 4>{99,9,7,99}), "SIMD gather mismatch");
    throws<std::out_of_range>([&] { (void)newton::simd::gather4_u64(table, {0,1,2,5}); });
}
void csv_mask_test() {
    std::mt19937 rng(42);
    for (std::size_t n = 0; n <= 64; ++n) for (bool initial : {false, true}) {
        std::string data(n, 'x');
        constexpr char chars[] = {'a', '"', ',', '\n', '\r', '\\', '0'};
        for (auto& ch : data) ch = chars[rng() % 7];
        bool inside = initial;
        std::uint64_t commas = 0, ends = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (data[i] == '"') inside = !inside;
            else if (!inside) {
                if (data[i] == ',') commas |= std::uint64_t{1} << i;
                if (data[i] == '\r' || data[i] == '\n') ends |= std::uint64_t{1} << i;
            }
        }
        const auto got = newton::parsing::csv_structure_mask64(data.data(), n, initial);
        check(got.commas == commas && got.line_ends == ends &&
              got.inside_quotes == inside, "Quote-mask parity mismatch");
    }
    throws<std::invalid_argument>([] {
        (void)newton::parsing::csv_structure_mask64(nullptr, 1);
    });
}
void numbers_test() {
    using namespace newton::parsing;
    check(parse_ascii_uint64("18446744073709551615") ==
          std::numeric_limits<std::uint64_t>::max(), "Max unsigned parse");
    check(!parse_ascii_uint64("18446744073709551616") &&
          !parse_ascii_uint64("+1") && !parse_ascii_uint64(""), "Unsigned overflow/invalid");
    check(parse_decimal_cents("5") == 500 && parse_decimal_cents("5.2") == 520 &&
          parse_decimal_cents("-0.03") == -3, "Decimal cents parse");
    check(parse_decimal_cents("-92233720368547758.08") ==
          std::numeric_limits<std::int64_t>::min(), "Min exact signed cents");
    check(parse_decimal_cents("92233720368547758.07") ==
          std::numeric_limits<std::int64_t>::max(), "Max exact signed cents");
    check(!parse_decimal_cents("1.999") && !parse_decimal_cents("1.") &&
          !parse_decimal_cents("1e3") && !parse_decimal_cents("92233720368547758.08"),
          "Decimal format and overflow");
    check(parse_timestamp_utc_seconds("1970-01-01T00:00:00Z") == 0 &&
          parse_timestamp_utc_seconds("1970-01-02T00:00:00Z") == 86400 &&
          parse_timestamp_utc_seconds("2000-02-29T12:00:00Z").has_value(),
          "Timestamp parse or leap year");
    check(!parse_timestamp_utc_seconds("2023-02-29T12:00:00Z") &&
          !parse_timestamp_utc_seconds("2020-13-01T00:00:00Z") &&
          !parse_timestamp_utc_seconds("1970-01-01T00:00:60Z"),
          "Timestamp invalid acceptance");
}
void csv_decode_test() {
    std::string row = "alpha,\"a\"\"b,cc\",5,\"hello\nworld\",";
    std::array<std::string_view, 6> fields{};
    std::array<char, 64> scratch{};
    const auto count = newton::parsing::decode_csv_fields_into(row, fields, scratch);
    check(count == 5 && fields[0] == "alpha" && fields[1] == "a\"b,cc" &&
          fields[2] == "5" && fields[3] == "hello\nworld" && fields[4].empty(),
          "CSV field decoding");
    throws<std::invalid_argument>([&] {
        (void)newton::parsing::decode_csv_fields_into("\"x\"z", fields, scratch);
    });
    throws<std::length_error>([&] {
        std::array<char, 2> tiny_scratch{};
        (void)newton::parsing::decode_csv_fields_into(row, fields, tiny_scratch);
    });
}
void packing_test() {
    std::array<std::uint64_t, 7> input{
        0, 1, 100000, std::numeric_limits<std::uint32_t>::max() - 1,
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint64_t>::max(), 77};
    std::array<std::uint32_t, 7> codes{};
    std::array<newton::memory::PackedException, 2> exceptions{};
    const auto n = newton::memory::pack_uint64_into(input, codes, exceptions);
    check(n == 2, "Packed exception count mismatch");
    for (std::size_t i = 0; i < input.size(); ++i)
        check(newton::memory::unpack_uint64_at(codes,
              std::span<const newton::memory::PackedException>(exceptions).first(n), i) ==
              input[i], "Packed roundtrip mismatch");
    throws<std::length_error>([&] {
        std::array<newton::memory::PackedException, 1> short_list{};
        (void)newton::memory::pack_uint64_into(input, codes, short_list);
    });
}
void flat_hash_test() {
    newton::analytics::FlatCountSum table(120);
    std::unordered_map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> expected;
    std::mt19937_64 rng(105);
    for (int i = 0; i < 8000; ++i) {
        const auto key = rng() % 101, value = rng() % 90;
        table.add(key, value);
        auto& e = expected[key];
        ++e.first;
        e.second += value;
    }
    check(table.distinct() == expected.size(), "Flat table distinct keys");
    for (const auto& [key, e] : expected) {
        const auto hit = table.find(key);
        check(hit && hit->count == e.first && hit->sum == e.second,
              "Flat table exact result mismatch");
    }
    const std::array<std::uint64_t, 6> left{3,3,8,7,99,1};
    const std::array<std::uint64_t, 7> right{3,7,3,3,9,99,8};
    std::vector<std::pair<std::size_t, std::size_t>> got, baseline;
    std::array<std::size_t, 16> heads{};
    std::array<std::size_t, 7> next{};
    const auto count = newton::analytics::flat_hash_join_into(left, right, heads, next,
        [&](std::size_t i, std::size_t j) { got.emplace_back(i, j); });
    for (std::size_t i = 0; i < left.size(); ++i)
        for (std::size_t j = 0; j < right.size(); ++j)
            if (left[i] == right[j]) baseline.emplace_back(i, j);
    std::sort(got.begin(), got.end());
    check(count == baseline.size() && got == baseline, "Flat duplicate join mismatch");

    // Full-multiset differential oracle for every caller bucket class:
    // power-of-two, odd, prime, singleton, many collisions and empty data.
    std::mt19937_64 join_rng{0x82f6b77d2026ULL};
    for (const std::size_t capacity : {1u, 2u, 3u, 7u, 16u, 31u, 64u, 127u, 256u}) {
        for (int trial = 0; trial < 96; ++trial) {
            const std::size_t nleft = static_cast<std::size_t>(trial * 17 % 67);
            const std::size_t nright = static_cast<std::size_t>(trial * 29 % 73);
            const auto domain = static_cast<std::uint64_t>(1 + trial % 43);
            std::vector<std::uint64_t> l(nleft), r(nright);
            for (auto& v : l)
                v = trial % 7 == 0 ? std::numeric_limits<std::uint64_t>::max()
                                   : join_rng() % domain;
            for (auto& v : r)
                v = trial % 7 == 0 ? std::numeric_limits<std::uint64_t>::max()
                                   : join_rng() % domain;
            std::vector<std::size_t> bucket_heads(capacity), links(nright);
            std::vector<std::pair<std::size_t, std::size_t>> actual, oracle;
            const auto found = newton::analytics::flat_hash_join_into(
                l, r, bucket_heads, links, [&](std::size_t i, std::size_t j) {
                    actual.emplace_back(i, j);
                });
            for (std::size_t i = 0; i < l.size(); ++i)
                for (std::size_t j = 0; j < r.size(); ++j)
                    if (l[i] == r[j]) oracle.emplace_back(i, j);
            std::sort(actual.begin(), actual.end());
            check(found == oracle.size() && actual == oracle,
                  "Optimized caller-owned hash join changed exact pairs");
        }
    }
    throws<std::length_error>([&] {
        std::array<std::size_t, 0> no_heads{};
        (void)newton::analytics::flat_hash_join_into(left, right,
            no_heads, next, [](std::size_t, std::size_t) {});
    });

}
void string_counter_test() {
    newton::analytics::InlineKeyCounter first(200), second(200);
    const std::string long_key(48, 'x');
    for (std::size_t i = 0; i < 100; ++i) {
        first.add("alpha", 2);
        first.add("beta", 4);
        second.add("alpha", 3);
        second.add(long_key, 5);
    }
    first.merge_from(second);
    check(first.distinct() == 3, "Merged inline-key distinct count");
    const auto a = first.find("alpha"), b = first.find("beta");
    const auto c = first.find(long_key);
    check(a.count == 200 && a.sum == 500 &&
          b.count == 100 && b.sum == 400 &&
          c.count == 100 && c.sum == 500, "Inline-key exact merge");
    check(first.find("not-here").count == 0, "Unknown inline key");
    first.clear();
    check(first.distinct() == 0 && first.find("alpha").count == 0,
          "Inline-key clear");
}
void fused_join_test() {
    const std::array<std::uint64_t, 4> left{3,4,3,7};
    const std::array<std::uint32_t, 4> groups{0,1,1,0};
    const std::array<std::uint64_t, 4> values{10,20,30,40};
    const std::array<std::uint64_t, 4> right{3,3,7,99};
    std::array<std::size_t, 8> heads{};
    std::array<std::size_t, 4> links{};
    std::array<std::uint64_t, 2> counts{}, sums{};
    const auto n = newton::analytics::fused_join_group_sum_into(
        left, groups, values, right, heads, links, counts, sums);
    check(n == 5 && counts[0] == 3 && counts[1] == 2 &&
          sums[0] == 60 && sums[1] == 60, "Fused join grouped result");
    newton::core::PhaseSample sample{"join"};
    { newton::core::ScopedPhase timing(sample); }
    check(sample.elapsed_ns < std::numeric_limits<std::uint64_t>::max(),
          "Phase timing resource");
}
void postings_test() {
    using newton::index::HybridPostings;
    std::mt19937 rng(123);
    for (std::size_t n : {1u, 32u, 65537u, 70000u}) {
        std::vector<std::uint32_t> ids;
        for (std::size_t i = 0; i < n; ++i)
            if (rng() % 5 == 0) ids.push_back(static_cast<std::uint32_t>(i));
        const auto posting = HybridPostings::from_sorted(ids, static_cast<std::uint32_t>(n));
        std::vector<std::uint32_t> roundtrip;
        posting.for_each([&](auto id) { roundtrip.push_back(id); });
        check(roundtrip == ids, "Postings roundtrip");
        for (std::size_t i = 0; i < n; ++i)
            check(posting.contains(static_cast<std::uint32_t>(i)) ==
                  std::binary_search(ids.begin(), ids.end(), static_cast<std::uint32_t>(i)),
                  "Hybrid contains incorrect");
    }
    // UINT32_MAX + 63 wrapped on 32-bit platforms, selecting an empty
    // bitmap and writing beyond it. Sparse high IDs must stay segmented.
    const std::array<std::uint32_t, 3> high_ids{
        0, 0xffff0000u, std::numeric_limits<std::uint32_t>::max() - 1
    };
    const auto high = HybridPostings::from_sorted(
        high_ids, std::numeric_limits<std::uint32_t>::max());
    check(high.format() == HybridPostings::Format::Segmented16 &&
          high.contains(high_ids[2]) &&
          !high.contains(std::numeric_limits<std::uint32_t>::max()),
          "Full uint32 universe must not overflow bitmap allocation");
    const std::array<std::uint32_t, 2> dense_ids{1, 3};
    const auto dense = HybridPostings::from_sorted(dense_ids, 64);
    check(dense.format() == HybridPostings::Format::Bitmap &&
          dense.contains(3) && !dense.contains(2),
          "Bitmap crossover changed");

    std::vector<std::uint32_t> a{1,2,5,50,51,800,1000};
    std::vector<std::uint32_t> b{0,2,4,50,52,53,800,850,900,1000};
    std::array<std::uint32_t, 7> result{};
    const auto count = newton::index::intersect_galloping_into(a,b,result);
    check(count == 4 && std::equal(result.begin(), result.begin() + 4,
          (std::array<std::uint32_t,4>{2,50,800,1000}).begin()), "Galloping intersection");
    newton::index::SelectiveTermIndex index;
    index.add_query("alpha beta");
    index.add_query("beta gamma");
    for (auto text : {"alpha beta", "Beta!", "GAMMA alpha beta", "delta", "gamma alpha"})
        (void)index.add_document(text);
    index.finalize();
    std::array<std::uint32_t, 5> matches{};
    const auto c1 = index.search_all_into("alpha beta", matches);
    check(c1 == 2 && matches[0] == 0 && matches[1] == 2, "Selective AND query");
    const auto c2 = index.search_all_into("alpha delta", matches);
    check(c2 == 0, "Selective unknown query");
}
void graph_test() {
    std::mt19937 rng(99);
    for (std::uint32_t run = 0; run < 12; ++run) {
        const std::uint32_t n = 4 + run * 7;
        newton::graph::GraphBuilder builder(n);
        for (std::uint32_t i = 0; i < n * 5; ++i)
            builder.add_edge(rng() % n, rng() % n, rng() % 500);
        const auto graph = builder.build();
        const auto reverse = newton::graph::make_reverse_graph(graph);
        const auto reorder = newton::graph::bfs_reorder_graph(graph);
        newton::graph::ShortestPathWorkspace reference(n);
        newton::graph::BidirectionalShortestPathWorkspace candidate(n);
        for (std::uint32_t i = 0; i < 120; ++i) {
            const std::uint32_t s = rng() % n, t = rng() % n;
            const auto expected = reference.query(graph,s,t);
            check(candidate.query(graph,reverse,s,t) == expected,
                  "Bidirectional Dijkstra differs from unidirectional reference");
            check(newton::graph::shortest_path(reorder.graph,
                reorder.old_to_new[s],reorder.old_to_new[t]) == expected,
                "BFS reordering changed graph path length");
        }
    }
}
} // namespace
int main() {
    try {
        scan_test(); csv_mask_test(); numbers_test(); csv_decode_test();
        packing_test(); flat_hash_test(); string_counter_test(); fused_join_test(); postings_test(); graph_test();
        const int prefetch_probe = 7;
        newton::parallel::prefetch_read(&prefetch_probe);
        std::cout << "Native optimization portfolio differential tests: PASS\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
