#pragma once
#include <newton/graph/csr_graph.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace newton::graph {

// Workload-specialized 0/1-weighted shortest paths (0-1 BFS).
// Validates all graph weights once; reuse for many queries on an immutable graph.
// Unlike general Dijkstra, no comparison heap, radix buckets or per-query O(V)
// initialization is required. Scratch belongs to the caller; not thread-safe.
// Construction: O(V+E). Query: O(V+E) for edges with weights in {0,1}.
// Memory: O(V+pending relaxations). Graph must outlive this workspace.
class ZeroOneShortestPathWorkspace {
public:
    explicit ZeroOneShortestPathWorkspace(const CsrGraph& graph)
        : graph_(&graph), distances_(graph.node_count()),
          active_epoch_(graph.node_count(), 0) {
        if (graph.node_count() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Too many graph nodes");
        for (std::size_t u = 0; u < graph.node_count(); ++u)
            for (const Edge& edge : graph.neighbors(static_cast<std::uint32_t>(u)))
                if (edge.weight > 1)
                    throw std::invalid_argument("ZeroOneShortestPathWorkspace requires 0/1 weights");
    }

    [[nodiscard]] std::optional<std::uint64_t> query(
        std::uint32_t source, std::uint32_t target) {
        if (source >= graph_->node_count() || target >= graph_->node_count())
            throw std::out_of_range("Invalid shortest-path source or target");
        if (source == target) return 0;

        if (++epoch_ == 0) {
            std::fill(active_epoch_.begin(), active_epoch_.end(), 0);
            epoch_ = 1;
        }
        pending_.clear();
        distances_[source] = 0;
        active_epoch_[source] = epoch_;
        pending_.emplace_front(0, source);

        while (!pending_.empty()) {
            const auto [distance, u] = pending_.front();
            pending_.pop_front();
            // Ignore previously queued suboptimal distances.
            if (active_epoch_[u] != epoch_ || distances_[u] != distance)
                continue;
            if (u == target) return distance;
            for (const Edge& edge : graph_->neighbors(u)) {
                // The prevalidated graph has only 0/1 weights. The shortest
                // simple path has < node_count() edges, so this cannot overflow.
                const auto candidate = distance + edge.weight;
                const auto v = edge.destination;
                if (active_epoch_[v] == epoch_ && candidate >= distances_[v])
                    continue;
                distances_[v] = candidate;
                active_epoch_[v] = epoch_;
                if (edge.weight == 0) pending_.emplace_front(candidate, v);
                else pending_.emplace_back(candidate, v);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::size_t node_count() const noexcept {
        return distances_.size();
    }

private:
    const CsrGraph* graph_;
    std::vector<std::uint64_t> distances_;
    std::vector<std::uint32_t> active_epoch_;
    std::uint32_t epoch_ = 0;
    std::deque<std::pair<std::uint64_t, std::uint32_t>> pending_;
};

} // namespace newton::graph
