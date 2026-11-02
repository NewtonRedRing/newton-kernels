#pragma once
#include <cstdint>
#include <span>
namespace newton::analytics {
// Both methods produce identical exact nearest-rank percentiles.
// Selection is usually preferable for a single percentile of a large group;
// full sorting can win for tiny groups or repeated percentile queries.
enum class PercentileStrategy { Selection, FullSort };
[[nodiscard]] std::uint64_t percentile_nearest_rank(
    std::span<const std::uint64_t> values, double q);
[[nodiscard]] std::uint64_t percentile_nearest_rank_using(
    std::span<const std::uint64_t> values, double q, PercentileStrategy strategy);
} // namespace newton::analytics
