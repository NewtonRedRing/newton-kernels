#pragma once
#include <newton/analytics/flat_hash.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace newton::analytics {
// Fused duplicate-preserving equijoin + per-group count/sum, with no
// materialized join pairs and no heap allocations. Keys have exact u64
// equality. Each left row contributes once per matching right row, including
// duplicates; group key refers to a dense caller-owned output bucket.
// Output arrays and caller-provided hash scratch must not alias inputs.
inline std::size_t fused_join_group_sum_into(
    std::span<const std::uint64_t> left_keys,
    std::span<const std::uint32_t> left_groups,
    std::span<const std::uint64_t> left_values,
    std::span<const std::uint64_t> right_keys,
    std::span<std::size_t> hash_heads,
    std::span<std::size_t> hash_next,
    std::span<std::uint64_t> group_counts,
    std::span<std::uint64_t> group_sums) {
    if (left_keys.size() != left_values.size() ||
        left_keys.size() != left_groups.size() ||
        group_counts.size() != group_sums.size())
        throw std::invalid_argument("Fused join array sizes differ");
    for (auto group : left_groups)
        if (static_cast<std::size_t>(group) >= group_counts.size())
            throw std::out_of_range("Fused join group key out of bounds");
    std::fill(group_counts.begin(), group_counts.end(), 0);
    std::fill(group_sums.begin(), group_sums.end(), 0);
    return flat_hash_join_into(left_keys, right_keys, hash_heads, hash_next,
        [&](std::size_t left, std::size_t) {
            const auto group = left_groups[left];
            const auto amount = left_values[left];
            if (group_counts[group] == std::numeric_limits<std::uint64_t>::max() ||
                amount > std::numeric_limits<std::uint64_t>::max() - group_sums[group])
                throw std::overflow_error("Fused join aggregation overflow");
            ++group_counts[group];
            group_sums[group] += amount;
        });
}
} // namespace newton::analytics
