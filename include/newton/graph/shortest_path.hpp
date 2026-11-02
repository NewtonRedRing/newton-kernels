#pragma once
#include <cstdint>
#include <optional>
namespace newton::graph {
class CsrGraph;
// Returns nullopt if target is unreachable or no representable distance
// exists. Throws on invalid node identifiers.
[[nodiscard]] std::optional<std::uint64_t> shortest_path(
    const CsrGraph& graph, std::uint32_t source, std::uint32_t target);
} // namespace newton::graph
