#include <newton/graph/csr_graph.hpp>
#include <newton/graph/shortest_path.hpp>
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
ShortestPathWorkspace::ShortestPathWorkspace(std::size_t n) : distances_(n), visited_at_(n, 0) {
    if (n > std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Too many graph nodes");
}
std::optional<std::uint64_t> ShortestPathWorkspace::query(
    const CsrGraph& graph, std::uint32_t source, std::uint32_t target,
    ShortestPathAlgorithm algorithm) {
    if (graph.node_count() != node_count()) throw std::invalid_argument("Workspace and graph node counts differ");
    if (source >= node_count() || target >= node_count()) throw std::out_of_range("Invalid source/target node");
    if (source == target) return 0;
    if (++epoch_ == 0) { std::fill(visited_at_.begin(), visited_at_.end(), 0); epoch_ = 1; }
    constexpr auto infinity = std::numeric_limits<std::uint64_t>::max();
    visited_at_[source] = epoch_;
    distances_[source] = 0;
    // Heap allocations remain reusable across queries.
    radix_.clear();
    binary_.clear();
    const bool use_radix = algorithm == ShortestPathAlgorithm::RadixHeap;
    if (use_radix) radix_.push(0, source);
    else { binary_.emplace_back(0, source); }
    const auto greater = [](const Entry& a, const Entry& b) { return a.first > b.first; };
    while (use_radix ? !radix_.empty() : !binary_.empty()) {
        Entry current;
        if (use_radix) current = radix_.pop();
        else {
            std::pop_heap(binary_.begin(), binary_.end(), greater);
            current = binary_.back();
            binary_.pop_back();
        }
        const auto [d, u] = current;
        if (visited_at_[u] != epoch_ || d != distances_[u]) continue;
        if (u == target) return d;
        for (const Edge& edge : graph.neighbors(u)) {
            if (edge.weight >= infinity - d) continue;
            const std::uint64_t candidate = d + edge.weight;
            const auto v = edge.destination;
            if (visited_at_[v] == epoch_ && candidate >= distances_[v]) continue;
            visited_at_[v] = epoch_;
            distances_[v] = candidate;
            if (use_radix) radix_.push(candidate, v);
            else {
                binary_.emplace_back(candidate, v);
                std::push_heap(binary_.begin(), binary_.end(), greater);
            }
        }
    }
    return std::nullopt;
}
std::optional<std::uint64_t> shortest_path(const CsrGraph& graph, std::uint32_t source, std::uint32_t target) {
    return ShortestPathWorkspace(graph.node_count()).query(graph, source, target);
}
std::optional<std::uint64_t> shortest_path_binary(const CsrGraph& graph, std::uint32_t source, std::uint32_t target) {
    return ShortestPathWorkspace(graph.node_count()).query(graph, source, target, ShortestPathAlgorithm::BinaryHeap);
}
} // namespace newton::graph
