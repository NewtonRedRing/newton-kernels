#pragma once
#include <newton/graph/csr_graph.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace newton::graph {

// Both passes use the existing CSR builder: count per-source degrees and
// prefix-sum/fill. Reverse graph construction is explicit and costed separately.
inline CsrGraph make_reverse_graph(const CsrGraph& graph) {
    GraphBuilder builder(graph.node_count());
    for (std::size_t u = 0; u < graph.node_count(); ++u)
        for (const auto& edge : graph.neighbors(static_cast<std::uint32_t>(u)))
            builder.add_edge(edge.destination, static_cast<std::uint32_t>(u), edge.weight);
    return builder.build();
}

// Optional locality-oriented BFS renumbering. Both permutations are retained
// so callers can translate IDs and recover original semantics exactly.
struct BfsReorderedGraph {
    CsrGraph graph;
    std::vector<std::uint32_t> old_to_new;
    std::vector<std::uint32_t> new_to_old;
};
inline BfsReorderedGraph bfs_reorder_graph(const CsrGraph& input) {
    const auto n = input.node_count();
    if (n > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Graph ID limit exceeded");
    constexpr auto missing = std::numeric_limits<std::uint32_t>::max();
    BfsReorderedGraph out;
    out.old_to_new.assign(n, missing);
    out.new_to_old.reserve(n);
    for (std::size_t start = 0; start < n; ++start) {
        if (out.old_to_new[start] != missing) continue;
        const auto root = static_cast<std::uint32_t>(start);
        out.old_to_new[start] = static_cast<std::uint32_t>(out.new_to_old.size());
        out.new_to_old.push_back(root);
        for (std::size_t cursor = out.new_to_old.size() - 1;
             cursor < out.new_to_old.size(); ++cursor) {
            const auto id = out.new_to_old[cursor];
            for (const auto& edge : input.neighbors(id)) {
                if (out.old_to_new[edge.destination] != missing) continue;
                out.old_to_new[edge.destination] =
                    static_cast<std::uint32_t>(out.new_to_old.size());
                out.new_to_old.push_back(edge.destination);
            }
        }
    }
    GraphBuilder builder(n);
    for (std::size_t u = 0; u < n; ++u)
        for (const auto& edge : input.neighbors(static_cast<std::uint32_t>(u)))
            builder.add_edge(out.old_to_new[u], out.old_to_new[edge.destination], edge.weight);
    out.graph = builder.build();
    return out;
}

// Per-caller reusable workspace. No threads or global state. Both input
// graphs must be paired forward/reverse adjacency with identical weights.
// Nonnegative 64-bit distances follow the existing non-representable-overflow
// convention. Each concurrent query needs its own workspace.
class BidirectionalShortestPathWorkspace {
    using Entry = std::pair<std::uint64_t, std::uint32_t>;
public:
    explicit BidirectionalShortestPathWorkspace(std::size_t n)
        : fdist_(n), bdist_(n), fseen_(n, 0), bseen_(n, 0) {
        if (n > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Graph workspace ID limit exceeded");
    }
    [[nodiscard]] std::optional<std::uint64_t> query(
        const CsrGraph& graph, const CsrGraph& reverse,
        std::uint32_t source, std::uint32_t target) {
        const auto n = fdist_.size();
        if (graph.node_count() != n || reverse.node_count() != n)
            throw std::invalid_argument("Graph/workspace sizes differ");
        if (source >= n || target >= n) throw std::out_of_range("Invalid graph endpoint");
        if (source == target) return std::uint64_t{0};
        if (++epoch_ == 0) {
            std::fill(fseen_.begin(), fseen_.end(), 0);
            std::fill(bseen_.begin(), bseen_.end(), 0);
            epoch_ = 1;
        }
        const auto less = [](const Entry& a, const Entry& b) {
            return a.first > b.first;
        };
        auto insert = [&](std::vector<Entry>& heap, Entry e) {
            heap.push_back(e);
            std::push_heap(heap.begin(), heap.end(), less);
        };
        auto take = [&](std::vector<Entry>& heap) {
            std::pop_heap(heap.begin(), heap.end(), less);
            const auto top = heap.back();
            heap.pop_back();
            return top;
        };
        const auto clean = [&](std::vector<Entry>& heap,
                               const std::vector<std::uint64_t>& dist,
                               const std::vector<std::uint32_t>& seen) {
            while (!heap.empty() &&
                   (seen[heap.front().second] != epoch_ ||
                    dist[heap.front().second] != heap.front().first))
                (void)take(heap);
        };
        forward_.clear();
        backward_.clear();
        fseen_[source] = epoch_;
        bseen_[target] = epoch_;
        fdist_[source] = 0;
        bdist_[target] = 0;
        insert(forward_, {0, source});
        insert(backward_, {0, target});
        constexpr auto inf = std::numeric_limits<std::uint64_t>::max();
        auto best = inf;
        while (true) {
            clean(forward_, fdist_, fseen_);
            clean(backward_, bdist_, bseen_);
            if (forward_.empty() || backward_.empty()) break;
            const auto fmin = forward_.front().first;
            const auto bmin = backward_.front().first;
            if (best != inf && (fmin >= best ||
                bmin >= best - fmin)) break;
            const bool forward = fmin <= bmin;
            auto& heap = forward ? forward_ : backward_;
            auto& dist = forward ? fdist_ : bdist_;
            auto& seen = forward ? fseen_ : bseen_;
            auto& other_dist = forward ? bdist_ : fdist_;
            auto& other_seen = forward ? bseen_ : fseen_;
            const auto& adjacency = forward ? graph : reverse;
            const auto [d, u] = take(heap);
            if (other_seen[u] == epoch_ && other_dist[u] < inf - d)
                best = std::min(best, d + other_dist[u]);
            for (const auto& edge : adjacency.neighbors(u)) {
                if (edge.weight >= inf - d) continue;
                const auto alt = d + edge.weight;
                const auto v = edge.destination;
                if (seen[v] == epoch_ && alt >= dist[v]) continue;
                seen[v] = epoch_;
                dist[v] = alt;
                insert(heap, {alt, v});
                if (other_seen[v] == epoch_ && other_dist[v] < inf - alt)
                    best = std::min(best, alt + other_dist[v]);
            }
        }
        return best == inf ? std::nullopt : std::optional<std::uint64_t>{best};
    }
private:
    std::vector<std::uint64_t> fdist_, bdist_;
    std::vector<std::uint32_t> fseen_, bseen_;
    std::uint32_t epoch_ = 0;
    std::vector<Entry> forward_, backward_;
};

inline std::optional<std::uint64_t> shortest_path_bidirectional(
    const CsrGraph& graph, const CsrGraph& reverse,
    std::uint32_t source, std::uint32_t target) {
    return BidirectionalShortestPathWorkspace(graph.node_count())
        .query(graph, reverse, source, target);
}
} // namespace newton::graph
