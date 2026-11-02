#include <newton/graph/csr_graph.hpp>
#include <newton/graph/shortest_path.hpp>
#include <newton/graph/radix_heap.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
namespace newton::graph {
std::span<const Edge> CsrGraph::neighbors(std::uint32_t node) const {
    if (node >= node_count()) throw std::out_of_range("Invalid graph node");
    return std::span<const Edge>(edges_).subspan(offsets_[node], offsets_[node + 1] - offsets_[node]);
}
GraphBuilder::GraphBuilder(std::size_t nodes) : nodes_(nodes) {
    if (nodes > std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Too many graph nodes");
}
void GraphBuilder::add_edge(std::uint32_t from, std::uint32_t to, std::uint64_t weight) {
    if (from >= nodes_ || to >= nodes_) throw std::out_of_range("Invalid graph edge endpoint");
    if (weight == std::numeric_limits<std::uint64_t>::max()) throw std::invalid_argument("Unsupported edge weight");
    edges_.push_back({from, {to, weight}});
}
CsrGraph GraphBuilder::build() const {
    CsrGraph graph;
    graph.offsets_.assign(nodes_ + 1, 0);
    for (const auto& edge : edges_) ++graph.offsets_[edge.from + 1];
    for (std::size_t i = 1; i < graph.offsets_.size(); ++i) graph.offsets_[i] += graph.offsets_[i - 1];
    graph.edges_.resize(edges_.size());
    auto cursor = graph.offsets_;
    for (const auto& raw : edges_) graph.edges_[cursor[raw.from]++] = raw.edge;
    return graph;
}
std::optional<std::uint64_t> shortest_path(const CsrGraph& graph, std::uint32_t source, std::uint32_t target) {
    if (source >= graph.node_count() || target >= graph.node_count()) throw std::out_of_range("Invalid source/target node");
    if (source == target) return 0;
    constexpr auto infinity = std::numeric_limits<std::uint64_t>::max();
    std::vector<std::uint64_t> distances(graph.node_count(), infinity);
    RadixHeap pending;
    distances[source] = 0;
    pending.push(0, source);
    while (!pending.empty()) {
        const auto [d, u] = pending.pop();
        if (d != distances[u]) continue;
        if (u == target) return d;
        for (const auto& edge : graph.neighbors(u)) {
            if (edge.weight >= infinity - d) continue; // non-representable candidate
            const auto candidate = d + edge.weight;
            if (candidate < distances[edge.destination]) {
                distances[edge.destination] = candidate;
                pending.push(candidate, edge.destination);
            }
        }
    }
    return std::nullopt;
}
} // namespace newton::graph
