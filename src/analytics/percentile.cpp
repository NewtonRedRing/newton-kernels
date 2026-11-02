#include <newton/analytics/percentile.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace newton::analytics {
std::uint64_t percentile_nearest_rank_using(
    std::span<const std::uint64_t> values, double q, PercentileStrategy strategy) {
    if (values.empty() || !std::isfinite(q) || q < 0.0 || q > 1.0)
        throw std::invalid_argument("Percentile requires non-empty values and q in [0, 1]");
    auto sample = std::vector<std::uint64_t>(values.begin(), values.end());
    const auto rank = static_cast<std::size_t>(std::ceil(q * static_cast<double>(sample.size())));
    const auto index = rank == 0 ? 0 : std::min(rank - 1, sample.size() - 1);
    if (strategy == PercentileStrategy::FullSort) {
        std::sort(sample.begin(), sample.end());
    } else {
        std::nth_element(sample.begin(), sample.begin() + static_cast<std::ptrdiff_t>(index), sample.end());
    }
    return sample[index];
}
std::uint64_t percentile_nearest_rank(std::span<const std::uint64_t> values, double q) {
    return percentile_nearest_rank_using(values, q, PercentileStrategy::Selection);
}
} // namespace newton::analytics
