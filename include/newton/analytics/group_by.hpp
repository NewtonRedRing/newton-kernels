#pragma once
#include <iterator>
#include <type_traits>
#include <unordered_map>
#include <utility>
namespace newton::analytics {
// Count by projected key; key must be hashable and copy/move-constructible.
template<class Range, class KeyFn>
auto group_count(const Range& rows, KeyFn key_fn) {
    using Key = std::decay_t<decltype(key_fn(*std::begin(rows)))>;
    std::unordered_map<Key, std::size_t> counts;
    for (const auto& row : rows) ++counts[key_fn(row)];
    return counts;
}
} // namespace newton::analytics
