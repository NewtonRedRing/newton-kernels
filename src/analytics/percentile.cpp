#include <newton/analytics/percentile.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <limits>
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
void percentiles_nearest_rank_bounded_into(
    std::span<const std::uint64_t> values, std::uint64_t minimum,
    std::span<std::size_t> counts_scratch,
    std::span<const double> qs, std::span<std::uint64_t> output) {
    if (values.empty() || counts_scratch.empty() || qs.size() != output.size())
        throw std::invalid_argument("Bounded percentile requires data, scratch and matching results");
    // Protect minimum + (number_of_buckets - 1) from unsigned overflow.
    if (counts_scratch.size() - 1 > std::numeric_limits<std::uint64_t>::max() - minimum)
        throw std::invalid_argument("Bounded percentile domain overflows uint64");
    for (const auto q : qs)
        if (!std::isfinite(q) || q < 0.0 || q > 1.0)
            throw std::invalid_argument("Percentile q must be finite and in [0,1]");

    std::fill(counts_scratch.begin(), counts_scratch.end(), 0);
    for (const auto value : values) {
        if (value < minimum || value - minimum >= counts_scratch.size())
            throw std::out_of_range("Value is outside percentile histogram domain");
        ++counts_scratch[static_cast<std::size_t>(value - minimum)];
    }
    // Prefix sums make each quantile query O(log(domain)) after one O(N) scan.
    std::size_t total = 0;
    for (auto& count : counts_scratch) {
        total += count;
        count = total;
    }
    for (std::size_t i = 0; i < qs.size(); ++i) {
        const auto rank = static_cast<std::size_t>(
            std::ceil(qs[i] * static_cast<double>(values.size())));
        const auto wanted = rank == 0 ? std::size_t{1} : std::min(rank, values.size());
        const auto it = std::lower_bound(counts_scratch.begin(), counts_scratch.end(), wanted);
        output[i] = minimum + static_cast<std::uint64_t>(it - counts_scratch.begin());
    }
}
} // namespace newton::analytics
