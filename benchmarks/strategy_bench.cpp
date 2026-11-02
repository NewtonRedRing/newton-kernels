#include <newton/graph/csr_graph.hpp>
#include <newton/graph/shortest_path.hpp>
#include <newton/index/inverted_index.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {
std::uint64_t rng = 0x4bfad9370cafeULL;
std::uint32_t random32() {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return static_cast<std::uint32_t>(rng >> 16);
}
template<class F> double measure_ms(F&& fn, int runs = 7) {
    std::vector<double> times;
    fn();  // warm up caches, code and allocations
    for (int i = 0; i < runs; ++i) {
        const auto start = std::chrono::steady_clock::now();
        fn();
        const auto end = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }
    std::sort(times.begin(), times.end());
    return times.at(times.size() / 2);
}
}
int main() {
    using namespace newton;
    constexpr std::uint32_t nodes = 20000, edges = 100000, queries = 180;
    graph::GraphBuilder builder(nodes);
    for (std::uint32_t i = 0; i < edges; ++i)
        builder.add_edge(random32() % nodes, random32() % nodes, 1 + random32() % 100);
    const auto g = builder.build();
    std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
    for (std::uint32_t i = 0; i < queries; ++i)
        pairs.emplace_back(random32() % nodes, random32() % nodes);
    graph::ShortestPathWorkspace radix(nodes), binary(nodes);
    std::uint64_t sink = 0;
    const auto radix_time = measure_ms([&] {
        std::uint64_t sum = 0;
        for (auto [s, t] : pairs) sum += radix.query(g, s, t).value_or(0);
        sink = sum;
    });
    const auto expected = sink;
    const auto binary_time = measure_ms([&] {
        std::uint64_t sum = 0;
        for (auto [s, t] : pairs)
            sum += binary.query(g, s, t, graph::ShortestPathAlgorithm::BinaryHeap).value_or(0);
        sink = sum;
    });
    if (expected != sink) return 2;
    const auto fresh_time = measure_ms([&] {
        std::uint64_t sum = 0;
        for (auto [s, t] : pairs) sum += graph::shortest_path(g, s, t).value_or(0);
        sink = sum;
    });
    if (expected != sink) return 3;
    std::cout << "Graph (" << nodes << " nodes, " << edges << " edges, " << queries << " queries; median of 7)\n"
              << "  reusable_radix_ms=" << radix_time << "\n"
              << "  reusable_binary_ms=" << binary_time << "\n"
              << "  fresh_radix_ms=" << fresh_time << "\n";
    index::InvertedIndex ix;
    for (int doc = 0; doc < 8000; ++doc) {
        std::string text;
        for (int token = 0; token < 30; ++token)
            text += ((random32() % 9) == 0 ? "rare" : "common") + std::string(" word") + std::to_string(random32() % 120) + " ";
        ix.add_document(text);
    }
    const std::string query = "common rare word87";
    const auto rare = measure_ms([&] {
        std::uint64_t sum = 0;
        for (int i = 0; i < 300; ++i)
            sum += ix.search_all(query, index::IntersectionStrategy::SmallestFirst).size();
        sink = sum;
    });
    const auto expected_index = sink;
    const auto ordered = measure_ms([&] {
        std::uint64_t sum = 0;
        for (int i = 0; i < 300; ++i)
            sum += ix.search_all(query, index::IntersectionStrategy::InputOrder).size();
        sink = sum;
    });
    if (expected_index != sink) return 4;
    std::cout << "Index (8000 docs, 300 AND queries; median of 7)\n"
              << "  smallest_first_ms=" << rare << "\n"
              << "  input_order_ms=" << ordered << "\n"
              << "checksum=" << sink << '\n';
}
