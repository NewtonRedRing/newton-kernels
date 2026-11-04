#pragma once
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace newton::analytics {

// Exact count by projected key. For sized ranges, reserve a bounded number
// of buckets before the hot ingestion loop, avoiding repeated rehashing while
// keeping the hint small even for a very large or highly duplicated input.
// The result and key semantics are unchanged; input can still be unsized.
template<class Range, class KeyFn>
auto group_count(const Range& rows, KeyFn key_fn) {
    using Key = std::decay_t<decltype(key_fn(*std::begin(rows)))>;
    std::unordered_map<Key, std::size_t> counts;
    if constexpr (requires { std::size(rows); }) {
        const auto n = static_cast<std::size_t>(std::size(rows));
        // 1024 buckets are an 8 KiB-sized pointer array on 64-bit machines.
        // Bound up-front work to avoid an O(n) zero-fill on duplicate-heavy
        // inputs; empty/small ranges retain the original allocation behavior.
        if (n >= 1024)
            counts.reserve(std::min<std::size_t>(n, 1024));
    }
    for (const auto& row : rows) ++counts[key_fn(row)];
    return counts;
}
} // namespace newton::analytics
