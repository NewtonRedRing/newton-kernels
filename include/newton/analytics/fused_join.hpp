#pragma once
#include <newton/analytics/flat_hash.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
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
    // Check all caller-owned scratch before resetting output arrays.
    // Invalid capacity is a precondition failure, not a partial aggregation.
    if (hash_heads.empty() || hash_next.size() < right_keys.size())
        throw std::length_error("Insufficient caller-owned join scratch");
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

// Opt-in strong output guarantee: stage both grouped arrays in caller-owned
// memory and commit only after all joins and checked arithmetic succeed.
// On exception, original output values are unchanged; staged arrays and hash
// links may have been modified. This never allocates, unlike copying outputs
// into temporary vectors, and does not change the default hot-path API.
// All scratch, outputs and input arrays must be disjoint.
inline std::size_t fused_join_group_sum_transactional_into(
    std::span<const std::uint64_t> left_keys,
    std::span<const std::uint32_t> left_groups,
    std::span<const std::uint64_t> left_values,
    std::span<const std::uint64_t> right_keys,
    std::span<std::size_t> hash_heads,
    std::span<std::size_t> hash_next,
    std::span<std::uint64_t> group_counts,
    std::span<std::uint64_t> group_sums,
    std::span<std::uint64_t> staged_counts,
    std::span<std::uint64_t> staged_sums) {
    if (group_counts.size() != staged_counts.size() ||
        group_sums.size() != staged_sums.size())
        throw std::invalid_argument("Transactional fused join stage sizes differ");

    // std::less provides a strict total ordering of unrelated pointers,
    // unlike built-in relational pointer comparisons.
    const auto overlaps = [](std::span<std::uint64_t> a,
                             std::span<std::uint64_t> b) noexcept {
        if (a.empty() || b.empty()) return false;
        const std::less<const std::uint64_t*> before{};
        return before(a.data(), b.data() + b.size()) &&
               before(b.data(), a.data() + a.size());
    };
    if (overlaps(group_counts, group_sums) ||
        overlaps(group_counts, staged_counts) ||
        overlaps(group_counts, staged_sums) ||
        overlaps(group_sums, staged_counts) ||
        overlaps(group_sums, staged_sums) ||
        overlaps(staged_counts, staged_sums))
        throw std::invalid_argument("Transactional fused join arrays overlap");

    const auto matches = fused_join_group_sum_into(
        left_keys, left_groups, left_values, right_keys, hash_heads, hash_next,
        staged_counts, staged_sums);
    std::copy(staged_counts.begin(), staged_counts.end(), group_counts.begin());
    std::copy(staged_sums.begin(), staged_sums.end(), group_sums.begin());
    return matches;
}
} // namespace newton::analytics
