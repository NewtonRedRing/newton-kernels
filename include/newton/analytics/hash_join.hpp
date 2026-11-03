#pragma once
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
namespace newton::analytics {
// Invokes visitor(left, right) for every matching pair, including duplicates.
template<class Left, class Right, class LeftKey, class RightKey, class Visitor>
void hash_inner_join(const Left& left, const Right& right,
                     LeftKey left_key, RightKey right_key, Visitor visitor) {
    using Key = std::decay_t<decltype(right_key(*std::begin(right)))>;
    using RightItem = typename Right::value_type;
    std::unordered_multimap<Key, const RightItem*> index;
    for (const auto& r : right) index.emplace(right_key(r), &r);
    for (const auto& l : left) {
        auto [first, last] = index.equal_range(left_key(l));
        for (; first != last; ++first) visitor(l, *first->second);
    }
}
} // namespace newton::analytics
