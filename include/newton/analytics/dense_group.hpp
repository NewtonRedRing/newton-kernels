#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace newton::analytics {
// Dense integer-key aggregations. The output index is the group key; the caller
// supplies storage for every key in [0, groups.size()). No heap allocations.
// All output slots are reset at the start of each operation. Invalid inputs may
// leave the output partially updated; they never cause an out-of-range access.
inline void dense_group_count_into(
    std::span<const std::uint32_t> keys,
    std::span<std::size_t> counts) {
    std::fill(counts.begin(), counts.end(), 0);
    for (const auto key : keys) {
        if (static_cast<std::size_t>(key) >= counts.size())
            throw std::out_of_range("Dense group key exceeds count array");
        ++counts[key];
    }
}

// Fuses count and unsigned sum in one pass. Counts and sums must be disjoint
// output arrays. Throws on overflow rather than silently changing arithmetic.
inline void dense_group_count_sum_into(
    std::span<const std::uint32_t> keys,
    std::span<const std::uint64_t> values,
    std::span<std::size_t> counts,
    std::span<std::uint64_t> sums) {
    if (keys.size() != values.size() || counts.size() != sums.size())
        throw std::invalid_argument("Dense aggregation input/output lengths differ");
    std::fill(counts.begin(), counts.end(), 0);
    std::fill(sums.begin(), sums.end(), 0);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto key = keys[i];
        if (static_cast<std::size_t>(key) >= counts.size())
            throw std::out_of_range("Dense group key exceeds output array");
        if (values[i] > std::numeric_limits<std::uint64_t>::max() - sums[key])
            throw std::overflow_error("Dense group sum overflow");
        ++counts[key];
        sums[key] += values[i];
    }
}
} // namespace newton::analytics
