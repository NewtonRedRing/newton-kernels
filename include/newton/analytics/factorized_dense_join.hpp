#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace newton::analytics {

// Exact, duplicate-preserving equijoin + per-group count/sum, without
// enumerating pair output. Use only when keys lie in a known bounded uint64
// interval and the requested result is an aggregate, not individual pairs.
//
// Right keys outside [lower_key, lower_key + frequency.size()) fail before
// modifying scratch/output; left keys outside the domain simply have no match.
// Caller owns frequency, group_counts, group_sums; none may alias the inputs.
// Counts and sums incorporate one contribution for every matching right row.
// Pair enumeration APIs remain available when actual pairs are required.
// O(domain width + left rows + right rows), independent of output cardinality.
// The exact overflow checks reject products and aggregate sums that cannot
// fit uint64_t; no silent wrapping or saturating results.
inline std::size_t factorized_dense_join_group_sum_into(
    std::span<const std::uint64_t> left_keys,
    std::span<const std::uint32_t> left_groups,
    std::span<const std::uint64_t> left_values,
    std::span<const std::uint64_t> right_keys,
    std::uint64_t lower_key,
    std::span<std::size_t> frequency,
    std::span<std::uint64_t> group_counts,
    std::span<std::uint64_t> group_sums) {
    if (left_keys.size() != left_groups.size() ||
        left_keys.size() != left_values.size() ||
        group_counts.size() != group_sums.size())
        throw std::invalid_argument("Factorized join input/output sizes differ");
    if (frequency.empty())
        throw std::length_error("Factorized join domain is empty");
    for (const auto group : left_groups)
        if (static_cast<std::size_t>(group) >= group_counts.size())
            throw std::out_of_range("Factorized join group outside output bounds");
    for (const auto key : right_keys)
        if (key < lower_key || key - lower_key >= frequency.size())
            throw std::out_of_range("Factorized join right key outside domain");

    std::fill(frequency.begin(), frequency.end(), std::size_t{0});
    for (const auto key : right_keys)
        ++frequency[static_cast<std::size_t>(key - lower_key)];
    std::fill(group_counts.begin(), group_counts.end(), std::uint64_t{0});
    std::fill(group_sums.begin(), group_sums.end(), std::uint64_t{0});

    std::size_t pairs = 0;
    constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t i = 0; i < left_keys.size(); ++i) {
        const auto key = left_keys[i];
        if (key < lower_key || key - lower_key >= frequency.size()) continue;
        const auto n = frequency[static_cast<std::size_t>(key - lower_key)];
        if (!n) continue;
        if (n > std::numeric_limits<std::size_t>::max() - pairs)
            throw std::overflow_error("Factorized join pair count overflow");
        const auto group = left_groups[i];
        if (n > limit - group_counts[group])
            throw std::overflow_error("Factorized join group count overflow");
        const auto value = left_values[i];
        if (value && n > limit / value)
            throw std::overflow_error("Factorized join group product overflow");
        const auto product = value * static_cast<std::uint64_t>(n);
        if (product > limit - group_sums[group])
            throw std::overflow_error("Factorized join group sum overflow");
        group_counts[group] += static_cast<std::uint64_t>(n);
        group_sums[group] += product;
        pairs += n;
    }
    return pairs;
}
} // namespace newton::analytics
