#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace newton::analytics {

// Adaptive exact aggregate-only equijoin. This deliberately never materializes
// matching pairs, so it cannot replace a pair-output join.
// Chooses a cache-local direct-address histogram when the observed right-key
// span fits the caller's dense scratch budget. Otherwise, it uses an exact-key
// sparse hash histogram. Both paths preserve duplicates and checked uint64
// count/sum semantics. Outputs are committed only after all checks succeed.
// Caller-owned outputs must not alias each other or the input arrays.
enum class AdaptiveJoinStrategy { DenseHistogram, SparseHash };
struct AdaptiveJoinResult {
    std::size_t matching_pairs = 0;
    AdaptiveJoinStrategy strategy = AdaptiveJoinStrategy::SparseHash;
    std::size_t dense_scratch_bytes = 0; // Zero when sparse hash is selected.
};
inline AdaptiveJoinResult factorized_adaptive_join_group_sum_into(
    std::span<const std::uint64_t> left_keys,
    std::span<const std::uint32_t> left_groups,
    std::span<const std::uint64_t> left_values,
    std::span<const std::uint64_t> right_keys,
    std::span<std::uint64_t> group_counts,
    std::span<std::uint64_t> group_sums,
    std::size_t max_dense_scratch_bytes = 4u * 1024u * 1024u) {
    if (left_keys.size() != left_groups.size() ||
        left_keys.size() != left_values.size() ||
        group_counts.size() != group_sums.size())
        throw std::invalid_argument("Adaptive join input/output sizes differ");
    for (const auto group : left_groups)
        if (static_cast<std::size_t>(group) >= group_counts.size())
            throw std::out_of_range("Adaptive join group out of bounds");

    std::vector<std::uint64_t> staged_counts(group_counts.size(),0);
    std::vector<std::uint64_t> staged_sums(group_sums.size(),0);
    AdaptiveJoinResult result;
    if (right_keys.empty() || left_keys.empty()) {
        std::fill(group_counts.begin(), group_counts.end(), 0);
        std::fill(group_sums.begin(), group_sums.end(), 0);
        return result;
    }

    const auto limits = std::minmax_element(right_keys.begin(),right_keys.end());
    const std::uint64_t minimum=*limits.first;
    const std::uint64_t maximum=*limits.second;
    const std::uint64_t distance=maximum-minimum; // No overflow; sorted range.
    const auto budget_slots=max_dense_scratch_bytes/sizeof(std::size_t);
    const bool use_dense=budget_slots>0 &&
        distance < budget_slots &&
        distance < std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> dense;
    std::unordered_map<std::uint64_t,std::size_t> sparse;
    if (use_dense) {
        const auto width=static_cast<std::size_t>(distance)+1;
        dense.assign(width,0);
        for (const auto key:right_keys)
            ++dense[static_cast<std::size_t>(key-minimum)];
        result.strategy=AdaptiveJoinStrategy::DenseHistogram;
        result.dense_scratch_bytes=dense.capacity()*sizeof(std::size_t);
    } else {
        // A bounded reserve avoids allocating right.size() buckets when
        // virtually every row contains one repeated key.
        sparse.reserve(std::min<std::size_t>(right_keys.size(),8192));
        for (const auto key:right_keys) ++sparse[key];
    }

    constexpr std::uint64_t limit=std::numeric_limits<std::uint64_t>::max();
    for (std::size_t i=0;i<left_keys.size();++i) {
        const auto key=left_keys[i];
        std::size_t n=0;
        if (use_dense) {
            if (key>=minimum && key-minimum<=distance)
                n=dense[static_cast<std::size_t>(key-minimum)];
        } else {
            const auto it=sparse.find(key);
            if (it!=sparse.end()) n=it->second;
        }
        if (!n) continue;
        const auto group=static_cast<std::size_t>(left_groups[i]);
        if (n>std::numeric_limits<std::size_t>::max()-result.matching_pairs)
            throw std::overflow_error("Adaptive join match count overflow");
        if (n>limit-staged_counts[group])
            throw std::overflow_error("Adaptive join group count overflow");
        const auto value=left_values[i];
        if (value && n>limit/value)
            throw std::overflow_error("Adaptive join group product overflow");
        const auto product=value*static_cast<std::uint64_t>(n);
        if (product>limit-staged_sums[group])
            throw std::overflow_error("Adaptive join group sum overflow");
        result.matching_pairs+=n;
        staged_counts[group]+=static_cast<std::uint64_t>(n);
        staged_sums[group]+=product;
    }
    std::copy(staged_counts.begin(),staged_counts.end(),group_counts.begin());
    std::copy(staged_sums.begin(),staged_sums.end(),group_sums.begin());
    return result;
}
} // namespace newton::analytics
