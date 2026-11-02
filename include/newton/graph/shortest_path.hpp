#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include <newton/graph/radix_heap.hpp>

namespace newton::graph {
class CsrGraph;

// Both algorithms compute exact nonnegative shortest-path distances.
// BinaryHeap often benefits short/sparse searches; RadixHeap exploits
// monotonically extracted unsigned integer distances.
enum class ShortestPathAlgorithm { RadixHeap, BinaryHeap };

// Reuse across queries on a fixed graph to avoid O(V) distance initialization.
// Not thread safe: each concurrent query requires a separate workspace.
class ShortestPathWorkspace {
public:
    explicit ShortestPathWorkspace(std::size_t node_count);
    [[nodiscard]] std::optional<std::uint64_t> query(
        const CsrGraph& graph, std::uint32_t source, std::uint32_t target,
        ShortestPathAlgorithm algorithm = ShortestPathAlgorithm::RadixHeap);
    [[nodiscard]] std::size_t node_count() const noexcept { return distances_.size(); }
private:
    using Entry = std::pair<std::uint64_t, std::uint32_t>;
    std::vector<std::uint64_t> distances_;
    std::vector<std::uint32_t> visited_at_;
    std::uint32_t epoch_ = 0;
    RadixHeap radix_;
    std::vector<Entry> binary_;
};

// Convenience API. Repeated queries should reuse ShortestPathWorkspace.
[[nodiscard]] std::optional<std::uint64_t> shortest_path(
    const CsrGraph& graph, std::uint32_t source, std::uint32_t target);
[[nodiscard]] std::optional<std::uint64_t> shortest_path_binary(
    const CsrGraph& graph, std::uint32_t source, std::uint32_t target);
} // namespace newton::graph
