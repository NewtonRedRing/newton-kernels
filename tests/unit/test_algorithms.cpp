#include <newton/graph/csr_graph.hpp>
#include <newton/graph/shortest_path.hpp>
#include <newton/index/inverted_index.hpp>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool valid, const char* why) { if (!valid) throw std::runtime_error(why); }
std::uint64_t state = 0xdeadbeef12345678ULL;
std::uint32_t random32() {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return static_cast<std::uint32_t>(state >> 16);
}
void graph_property() {
    constexpr std::uint32_t nodes = 180;
    newton::graph::GraphBuilder builder(nodes);
    std::vector<std::vector<std::pair<std::uint32_t, std::uint64_t>>> links(nodes);
    for (int i = 0; i < 1000; ++i) {
        const auto u = random32() % nodes, v = random32() % nodes;
        const auto weight = random32() % 350;
        builder.add_edge(u, v, weight);
        links[u].push_back({v, weight});
    }
    const auto graph = builder.build();
    newton::graph::ShortestPathWorkspace reusable(nodes);
    for (std::uint32_t source = 0; source < nodes; source += 17) {
        // Independent O(V^2 + E) reference; no priority queues.
        std::vector<std::uint64_t> dist(nodes, std::numeric_limits<std::uint64_t>::max());
        std::vector<bool> settled(nodes, false);
        dist[source] = 0;
        for (std::uint32_t k = 0; k < nodes; ++k) {
            std::uint32_t best = nodes;
            for (std::uint32_t j = 0; j < nodes; ++j)
                if (!settled[j] && dist[j] != std::numeric_limits<std::uint64_t>::max() &&
                    (best == nodes || dist[j] < dist[best])) best = j;
            if (best == nodes) break;
            settled[best] = true;
            for (auto [to, weight] : links[best])
                dist[to] = std::min(dist[to], dist[best] + weight);
        }
        for (std::uint32_t target = 0; target < nodes; ++target) {
            std::optional<std::uint64_t> expected;
            if (dist[target] != std::numeric_limits<std::uint64_t>::max()) expected = dist[target];
            require(reusable.query(graph, source, target) == expected, "reusable radix mismatch");
            require(reusable.query(graph, source, target, newton::graph::ShortestPathAlgorithm::BinaryHeap) == expected,
                    "reusable binary mismatch");
        }
    }
    require(reusable.node_count() == nodes, "workspace size");
    bool rejected = false;
    try { (void)reusable.query(graph, nodes, 0); } catch (const std::out_of_range&) { rejected = true; }
    require(rejected, "invalid source should be rejected");
    rejected = false;
    try { (void)newton::graph::ShortestPathWorkspace(nodes + 1).query(graph, 0, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "incompatible workspace should be rejected");
    newton::graph::GraphBuilder overflow(3);
    overflow.add_edge(0, 1, std::numeric_limits<std::uint64_t>::max() - 3);
    overflow.add_edge(1, 2, 10);
    const auto extreme = overflow.build();
    require(!newton::graph::shortest_path(extreme, 0, 2), "overflow path should be absent");
    require(!newton::graph::shortest_path_binary(extreme, 0, 2), "overflow binary path should be absent");
    require(newton::graph::shortest_path_binary(extreme, 0, 1) ==
            std::numeric_limits<std::uint64_t>::max() - 3, "64-bit weight truncated");
}
void index_property() {
    newton::index::InvertedIndex index;
    constexpr int docs = 250, length = 24;
    std::vector<std::vector<std::string>> source;
    for (int d = 0; d < docs; ++d) {
        std::string line;
        std::vector<std::string> words;
        for (int j = 0; j < length; ++j) {
            const auto x = random32() % 12;
            const auto word = std::string("w") + std::to_string(x);
            words.push_back(word);
            if (!line.empty()) line += ' ';
            line += word;
        }
        require(index.add_document(line) == static_cast<std::uint32_t>(d), "document numbering");
        source.push_back(std::move(words));
    }
    for (int k = 0; k < 100; ++k) {
        const int a = random32() % 12, b = random32() % 12, c = random32() % 12;
        const auto word_a = std::string("w") + std::to_string(a);
        const auto word_b = std::string("w") + std::to_string(b);
        const auto word_c = std::string("w") + std::to_string(c);
        const auto query = word_a + " " + word_b + " " + word_c;
        std::vector<std::uint32_t> all, phrase;
        for (std::uint32_t d = 0; d < source.size(); ++d) {
            const auto& w = source[d];
            if (std::find(w.begin(), w.end(), word_a) != w.end() &&
                std::find(w.begin(), w.end(), word_b) != w.end() &&
                std::find(w.begin(), w.end(), word_c) != w.end()) all.push_back(d);
            bool match = false;
            for (std::size_t i = 0; i + 2 < w.size(); ++i) {
                if (w[i] == word_a && w[i + 1] == word_b && w[i + 2] == word_c) { match = true; break; }
            }
            if (match) phrase.push_back(d);
        }
        using newton::index::IntersectionStrategy;
        require(index.search_all(query, IntersectionStrategy::SmallestFirst) == all, "rarest-first AND mismatch");
        require(index.search_all(query, IntersectionStrategy::InputOrder) == all, "input-order AND mismatch");
        require(index.search_phrase(query) == phrase, "anchored phrase mismatch");
    }
}
} // namespace
int main() {
    try { graph_property(); index_property(); std::cout << "Graph/index differential tests: PASS\n"; }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
