#include <newton/analytics/group_by.hpp>
#include <newton/analytics/hash_join.hpp>
#include <newton/analytics/grouped_hash_join.hpp>
#include <newton/analytics/percentile.hpp>
#include <newton/core/cpu_features.hpp>
#include <newton/graph/csr_graph.hpp>
#include <newton/graph/radix_heap.hpp>
#include <newton/graph/shortest_path.hpp>
#include <newton/index/inverted_index.hpp>
#include <newton/memory/aligned_buffer.hpp>
#include <newton/memory/mapped_file.hpp>
#include <newton/parallel/executor.hpp>
#include <newton/parsing/csv_parser.hpp>
#include <newton/parsing/delimited_reader.hpp>
#include <newton/simd/byte_scan.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
int assertions = 0;
void check(bool ok, const char* what) {
    ++assertions;
    if (!ok) throw std::runtime_error(what);
}
template<class F> void throws(F fn, const char* what) {
    try { fn(); } catch (const std::exception&) { ++assertions; return; }
    throw std::runtime_error(what);
}
void test_simd() {
    using namespace newton::simd;
    check(find_byte({}, ',') == 0, "empty find");
    check(count_byte({}, ',') == 0, "empty count");
    check(find_byte("hello,world", ',') == 5, "first byte");
    check(find_byte("hello", ',') == 5, "absent byte");
    std::string s(300, 'z'); s[257] = ','; s[288] = ',';
    check(find_byte(s, ',') == 257, "simd find tail");
    check(count_byte(s, ',') == 2, "simd count");
    for (auto strategy : {ByteScanStrategy::Auto, ByteScanStrategy::Scalar,
                          ByteScanStrategy::Avx2, ByteScanStrategy::Avx512}) {
        check(find_byte_using(s, ',', strategy) == 257, "strategy find");
        check(count_byte_using(s, ',', strategy) == 2, "strategy count");
        check(find_byte_using({}, ',', strategy) == 0, "strategy empty find");
        check(count_byte_using({}, ',', strategy) == 0, "strategy empty count");
        check(find_byte_using("abc", ',', strategy) == 3, "strategy absent find");
        std::string binary(1027, '\0');
        binary[1026] = '\x7f';
        check(find_byte_using(binary, '\x7f', strategy) == 1026, "strategy binary tail");
        check(count_byte_using(binary, '\0', strategy) == 1026, "strategy NUL scan");
    }

    // Differential scan oracle over every vector-tail length, seven
    // unaligned starts, high bytes and embedded NULs.
    for (std::size_t n = 0; n <= 257; ++n) {
        std::string backing(n + 64, '\0');
        for (std::size_t i = 0; i < backing.size(); ++i)
            backing[i] = static_cast<char>((i * 37 + n * 19) & 255);
        for (const std::size_t offset : {0u, 1u, 7u, 15u, 31u, 32u, 63u}) {
            const std::string_view view(backing.data() + offset, n);
            for (const char needle : {'\0', ',', '\x7f', '\xff'}) {
                const auto where = view.find(needle);
                const auto expected_find = where == std::string_view::npos ? n : where;
                const auto expected_count = static_cast<std::size_t>(
                    std::count(view.begin(), view.end(), needle));
                for (auto strategy : {ByteScanStrategy::Auto, ByteScanStrategy::Scalar,
                                      ByteScanStrategy::Avx2, ByteScanStrategy::Avx512}) {
                    check(find_byte_using(view, needle, strategy) == expected_find,
                          "SIMD unaligned first-match parity mismatch");
                    check(count_byte_using(view, needle, strategy) == expected_count,
                          "SIMD unaligned count parity mismatch");
                }
            }
        }
    }

    const auto snapshot = newton::core::cpu_features();
    for (int i = 0; i < 1024; ++i) {
        const auto next = newton::core::cpu_features();
        check(next.sse42 == snapshot.sse42 && next.avx2 == snapshot.avx2 &&
              next.avx512f == snapshot.avx512f && next.avx512bw == snapshot.avx512bw,
              "CPU snapshot changed across repeated probes");
    }
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
    __builtin_cpu_init();
    check(snapshot.sse42 == (__builtin_cpu_supports("sse4.2") != 0) &&
          snapshot.avx2 == (__builtin_cpu_supports("avx2") != 0) &&
          snapshot.avx512f == (__builtin_cpu_supports("avx512f") != 0) &&
          snapshot.avx512bw == (__builtin_cpu_supports("avx512bw") != 0),
          "CPU snapshot differs from process-usable compiler flags");
#endif
}
void test_csv() {
    using namespace newton::parsing;
    CsvParser parser;
    std::vector<CsvRow> rows;
    const auto count = parser.parse("name,comment\r\n\"A,B\",\"quote \"\"x\"\"\"\r\nx,\"line\nnext\"", [&](const CsvRow& r) { rows.push_back(r); });
    check(count == 3 && rows.size() == 3, "CSV count");
    check(rows[1][0] == "A,B" && rows[1][1] == "quote \"x\"", "CSV quotes");
    check(rows[2][1] == "line\nnext", "CSV embedded newline");
    check(parser.parse("", [](const CsvRow&) {}) == 0, "CSV empty");
    throws([&] { (void)parser.parse("\"bad", [](const CsvRow&) {}); }, "unclosed quote");
    throws([&] { (void)parser.parse("a\"b", [](const CsvRow&) {}); }, "invalid quote");
    std::vector<std::vector<std::string>> simple;
    for_each_delimited_record("a;b;\r\nc;;d\n", ';', [&](auto f) {
        simple.emplace_back(f.begin(), f.end());
    });
    check(simple.size() == 2 && simple[0].size() == 3 && simple[0][2].empty(), "delimited trailing field");
    check(simple[1].size() == 3 && simple[1][1].empty(), "delimited empty field");
}
void test_index() {
    newton::index::InvertedIndex ix;
    check(ix.add_document("Red blue RED") == 0, "index id 0");
    check(ix.add_document("blue green") == 1, "index id 1");
    check(ix.add_document("red blue green") == 2, "index id 2");
    check((ix.search_all("RED blue") == std::vector<std::uint32_t>{0, 2}), "index AND");
    check((ix.search_phrase("red blue") == std::vector<std::uint32_t>{0, 2}), "index phrase");
    check(ix.search_phrase("green red").empty(), "index no phrase");
    check(ix.search_all("missing").empty(), "index missing");
}
void test_graph() {
    using namespace newton::graph;
    GraphBuilder builder(5);
    builder.add_edge(0, 1, 10);
    builder.add_edge(0, 2, 2);
    builder.add_edge(2, 1, 1);
    builder.add_edge(1, 3, 4);
    const auto graph = builder.build();
    check(graph.node_count() == 5 && graph.edge_count() == 4, "CSR counts");
    check(graph.neighbors(4).empty(), "CSR empty neighbors");
    check(shortest_path(graph, 0, 3).value() == 7, "Dijkstra path");
    check(shortest_path(graph, 4, 4).value() == 0, "Dijkstra identity");
    check(!shortest_path(graph, 0, 4), "unreachable target");
    throws([&] { builder.add_edge(9, 0, 1); }, "out of bounds edge");
    RadixHeap heap;
    heap.push(3, 1); heap.push(12, 2); heap.push(3, 3);
    check(heap.pop().first == 3 && heap.pop().first == 3 && heap.pop().first == 12, "heap sorted");
    throws([&] { heap.push(11, 4); }, "heap monotone invariant");
}
void test_analytics() {
    using namespace newton::analytics;
    const std::array<std::uint64_t, 5> values{10, 50, 20, 40, 30};
    check(percentile_nearest_rank(values, 0.0) == 10, "percentile zero");
    check(percentile_nearest_rank(values, 0.5) == 30, "percentile median");
    check(percentile_nearest_rank(values, 1.0) == 50, "percentile max");
    for (const auto q : {0.0, 0.01, 0.5, 0.9, 1.0})
        check(percentile_nearest_rank_using(values, q, PercentileStrategy::Selection) ==
              percentile_nearest_rank_using(values, q, PercentileStrategy::FullSort),
              "percentile strategies disagree");
    const std::vector<std::uint64_t> duplicates{3, 3, 9, 1, 1, 7, 7};
    for (const auto q : {0.0, 0.25, 0.5, 0.99, 1.0})
        check(percentile_nearest_rank_using(duplicates, q, PercentileStrategy::Selection) ==
              percentile_nearest_rank_using(duplicates, q, PercentileStrategy::FullSort),
              "percentile duplicate rank mismatch");
    throws([&] { (void)percentile_nearest_rank(values, 2.0); }, "invalid percentile");
    const std::vector<int> data{1, 1, 2};
    const auto groups = group_count(data, [](int n) { return n; });
    check(groups.at(1) == 2 && groups.at(2) == 1, "group count");
    const std::vector<std::pair<int, int>> left{{1, 5}, {2, 6}};
    const std::vector<std::pair<int, int>> right{{1, 7}, {1, 8}, {9, 10}};
    int total = 0;
    hash_inner_join(left, right, [](const auto& p) { return p.first; }, [](const auto& p) { return p.first; },
        [&](const auto& l, const auto& r) { total += l.second + r.second; });
    check(total == 25, "duplicate-preserving join");

    // Differential comparison against a quadratic exact-key oracle. Check the
    // full multiset of row identities, not an order-dependent rolling digest.
    for (std::uint32_t trial = 0; trial < 128; ++trial) {
        std::uint64_t rng = 0x9e3779b97f4a7c15ULL + trial;
        const auto next = [&rng]() {
            rng ^= rng << 13;
            rng ^= rng >> 7;
            rng ^= rng << 17;
            return rng;
        };
        const std::uint32_t domain = 1 + trial % 73;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> a, b;
        for (std::uint32_t i = 0; i < trial % 89; ++i)
            a.emplace_back(static_cast<std::uint32_t>(next() % domain), i);
        for (std::uint32_t i = 0; i < (trial * 7) % 113; ++i)
            b.emplace_back(static_cast<std::uint32_t>(next() % domain), i);
        std::vector<std::pair<std::uint32_t, std::uint32_t>> expected, actual;
        for (const auto& lhs : a)
            for (const auto& rhs : b)
                if (lhs.first == rhs.first)
                    expected.emplace_back(lhs.second, rhs.second);
        hash_inner_join_grouped(a, b, [](const auto& r) { return r.first; },
            [](const auto& r) { return r.first; },
            [&](const auto& lhs, const auto& rhs) {
                actual.emplace_back(lhs.second, rhs.second);
            });
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(), actual.end());
        check(actual == expected, "optimized hash join lost or duplicated a pair");
    }
    const std::vector<std::pair<std::string, int>> words_left{
        {"same", 1}, {"missing", 2}, {"same", 3}};
    const std::vector<std::pair<std::string, int>> words_right{
        {"same", 10}, {"same", 20}, {"other", 30}};
    std::vector<std::pair<int, int>> word_pairs;
    hash_inner_join_grouped(words_left, words_right,
        [](const auto& x) { return x.first; },
        [](const auto& x) { return x.first; },
        [&](const auto& a, const auto& b) {
            word_pairs.emplace_back(a.second, b.second);
        });
    std::sort(word_pairs.begin(), word_pairs.end());
    check(word_pairs == std::vector<std::pair<int, int>>{
        {1, 10}, {1, 20}, {3, 10}, {3, 20}},
        "optimized hash join failed full string equality");

}
void test_memory() {
    newton::memory::AlignedBuffer<int, 64> buf(17);
    check(buf.size() == 17 && reinterpret_cast<std::uintptr_t>(buf.data()) % 64 == 0, "aligned allocation");

    newton::memory::AlignedBuffer<int, 64> empty(0);
    check(empty.data() == nullptr && empty.size() == 0 && empty.span().empty(),
          "zero-sized aligned buffer must not allocate");
    newton::memory::AlignedBuffer<int, 64> moving(9);
    auto* original = moving.data();
    newton::memory::AlignedBuffer<int, 64> destination(2);
    destination = std::move(moving);
    check(destination.data() == original && destination.size() == 9 &&
          moving.data() == nullptr && moving.size() == 0,
          "aligned buffer move assignment lost or double-owned storage");

    const auto path = std::filesystem::temp_directory_path() / "newton-kernels-mapping-unit.tmp";
    { std::ofstream out(path, std::ios::binary); out << "abc\n"; }
    try {
        newton::memory::MappedFile map(path);
        check(map.size() == 4 && map.text() == "abc\n", "mapped contents");
        auto moved = std::move(map);
        check(moved.text() == "abc\n" && map.size() == 0, "mapped move");
    } catch (...) { std::filesystem::remove(path); throw; }
    std::filesystem::remove(path);
}
void test_parallel() {
    newton::parallel::Executor executor(2);
    check(executor.size() == 2, "worker count");
    auto answer = executor.submit([] { return 42; });
    check(answer.get() == 42, "future result");
    std::atomic<int> visits{0};
    executor.parallel_for(0, 100, 9, [&](std::size_t a, std::size_t b) { visits += static_cast<int>(b - a); });
    check(visits == 100, "parallel partitions");
    throws([&] { executor.parallel_for(0, 10, 0, [](auto, auto) {}); }, "invalid grain");
    auto failed = executor.submit([]() -> void { throw std::runtime_error("intentional"); });
    throws([&] { failed.get(); }, "future exception");

    // An executor worker waiting on its own queued tasks used to deadlock.
    newton::parallel::Executor single_worker(1);
    auto nested = single_worker.submit([&single_worker] {
        std::size_t total = 0;
        single_worker.parallel_for(0, 37, 4,
            [&](std::size_t start, std::size_t finish) { total += finish - start; });
        return total;
    });
    check(nested.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
          "reentrant parallel_for deadlocked on a single worker");
    check(nested.get() == 37, "reentrant parallel_for incorrect result");

    // A callback exception must not abandon other borrowed callbacks.
    std::atomic<int> completed{0};
    throws([&] {
        single_worker.parallel_for(0, 10, 1, [&](std::size_t start, std::size_t) {
            ++completed;
            if (start == 4) throw std::runtime_error("expected callback failure");
        });
    }, "parallel_for failed to propagate callback exception");
    check(completed == 10, "parallel_for returned before in-flight callbacks ended");

    // Reject impossible work counts before submitting *any* borrowed tasks.
    std::atomic<int> unexpectedly_run{0};
    throws([&] {
        single_worker.parallel_for(0, std::numeric_limits<std::size_t>::max(),
            1, [&](std::size_t, std::size_t) { ++unexpectedly_run; });
    }, "parallel_for should reject an unrepresentable futures allocation");
    check(unexpectedly_run == 0, "parallel_for submitted work before failed reserve");
}
}
int main() {
    try {
        test_simd(); test_csv(); test_index(); test_graph();
        test_analytics(); test_memory(); test_parallel();
        std::cout << "PASS " << assertions << " assertions\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
