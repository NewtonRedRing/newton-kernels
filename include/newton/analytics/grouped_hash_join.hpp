#pragma once
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
namespace newton::analytics {
// Opt-in grouped join. Default hash_inner_join retains its established implementation.
// Invokes visitor(left, right) for every matching pair, including duplicates.
// Pair traversal order is unspecified; each right row is visited exactly once
// for every matching left row. Index entries hold borrowed right-row addresses.
template<class Left, class Right, class LeftKey, class RightKey, class Visitor>
void hash_inner_join_grouped(const Left& left, const Right& right,
                     LeftKey left_key, RightKey right_key, Visitor visitor) {
    using Key = std::decay_t<decltype(right_key(*std::begin(right)))>;
    using RightItem = typename Right::value_type;
    // Group duplicates in contiguous vectors instead of allocating a separate
    // unordered_multimap node for every right row. Exact key equality is still
    // checked by unordered_map; neither hashes nor fingerprints are identities.
    std::unordered_map<Key, std::vector<const RightItem*>> index;
    if constexpr (requires { std::size(right); })
        index.reserve(std::min<std::size_t>(std::size(right), 2048));
    for (const auto& r : right)
        index[right_key(r)].push_back(&r);
    for (const auto& l : left) {
        const auto it = index.find(left_key(l));
        if (it == index.end()) continue;
        for (const RightItem* r : it->second) visitor(l, *r);
    }
}
} // namespace newton::analytics
