#pragma once
#include <cstdint>
#include <cstddef>
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
// Exact nearest-rank percentiles for values in the bounded domain
// [minimum, minimum + counts_scratch.size()). Reuses caller-owned histogram
// memory; neither input values nor quantiles are modified and no heap memory
// is allocated. The histogram is overwritten with inclusive prefix counts.
// All quantiles must be finite and in [0,1], and output size must match qs.
// Use this only when the value domain is small enough to make histogram
// construction and clearing worthwhile; the general percentile API remains
// preferable for wide/sparse domains.
void percentiles_nearest_rank_bounded_into(
    std::span<const std::uint64_t> values,
    std::uint64_t minimum,
    std::span<std::size_t> counts_scratch,
    std::span<const double> qs,
    std::span<std::uint64_t> output);
} // namespace newton::analytics
