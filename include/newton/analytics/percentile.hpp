#pragma once
#include <cstdint>
#include <span>
namespace newton::analytics {
// Exact nearest-rank percentile (q is in [0,1]); input remains unchanged.
// Throws std::invalid_argument on empty input or invalid q.
[[nodiscard]] std::uint64_t percentile_nearest_rank(std::span<const std::uint64_t> values, double q);
} // namespace newton::analytics
