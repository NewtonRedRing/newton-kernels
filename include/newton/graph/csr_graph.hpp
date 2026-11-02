#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace newton::graph {
struct Edge { std::uint32_t destination; std::uint64_t weight; };
class CsrGraph {
public:
    [[nodiscard]] std::size_t node_count() const noexcept { return offsets_.empty() ? 0 : offsets_.size() - 1; }
    [[nodiscard]] std::size_t edge_count() const noexcept { return edges_.size(); }
    [[nodiscard]] std::span<const Edge> neighbors(std::uint32_t node) const;
private:
    friend class GraphBuilder;
    std::vector<std::size_t> offsets_;
    std::vector<Edge> edges_;
};
class GraphBuilder {
public:
    explicit GraphBuilder(std::size_t nodes);
    void add_edge(std::uint32_t from, std::uint32_t to, std::uint64_t weight);
    [[nodiscard]] CsrGraph build() const;
private:
    struct RawEdge { std::uint32_t from; Edge edge; };
    std::size_t nodes_;
    std::vector<RawEdge> edges_;
};
} // namespace newton::graph
