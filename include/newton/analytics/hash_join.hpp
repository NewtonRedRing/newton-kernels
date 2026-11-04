#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace newton::analytics {

// Invokes visitor(left, right) for every matching pair, including duplicates.
// Full key equality is retained; pair traversal order is unspecified.
// Integral keys in a dense bounded domain use a CSR-like, histogram/scatter
// inverted index with a contiguous row-reference array and no per-row nodes.
// All other cases retain the unordered_multimap implementation.
template<class Left, class Right, class LeftKey, class RightKey, class Visitor>
void hash_inner_join(const Left& left, const Right& right,
                     LeftKey left_key, RightKey right_key, Visitor visitor) {
    using Key = std::decay_t<decltype(right_key(*std::begin(right)))>;
    using LeftProjected = std::decay_t<decltype(left_key(*std::begin(left)))>;
    using RightItem = typename Right::value_type;

    if constexpr (std::is_integral_v<Key> && !std::is_same_v<Key, bool> &&
                  std::is_same_v<Key, LeftProjected> && sizeof(Key) <= 8 &&
                  requires { std::size(right); }) {
        const auto n = static_cast<std::size_t>(std::size(right));
        if (n >= 256 && n <= 262144) {
            struct RowRef { Key key; const RightItem* row; };
            std::vector<RowRef> keyed;
            keyed.reserve(n);
            Key min_key{}, max_key{};
            bool first = true;
            // Evaluate projections once per right row; preserve borrowed
            // right-row identity even when different rows share the key.
            for (const RightItem& row : right) {
                const Key key = right_key(row);
                keyed.push_back({key, &row});
                if (first) { min_key = max_key = key; first = false; }
                else {
                    if (key < min_key) min_key = key;
                    if (max_key < key) max_key = key;
                }
            }
            // uint64_t arithmetic is modulo 2^64, so subtraction remains
            // defined even when the signed key range crosses zero or INT_MIN.
            if (!first) {
                const auto spread = static_cast<std::uint64_t>(max_key) -
                                    static_cast<std::uint64_t>(min_key);
                if (spread < 8192 && keyed.size() * 8 >= spread + 1) {
                    const auto width = static_cast<std::size_t>(spread + 1);
                    std::vector<std::size_t> counts(width, 0);
                    const auto bucket = [min_key](Key k) noexcept {
                        return static_cast<std::size_t>(
                            static_cast<std::uint64_t>(k) -
                            static_cast<std::uint64_t>(min_key));
                    };
                    for (const RowRef& entry : keyed) ++counts[bucket(entry.key)];

                    std::vector<std::size_t> offsets(width + 1, 0);
                    for (std::size_t i = 0; i < width; ++i)
                        offsets[i + 1] = offsets[i] + counts[i];
                    std::copy(offsets.begin(), offsets.end() - 1, counts.begin());

                    // Contiguous posting lists improve cache locality and
                    // avoid per-key vector/node allocation in the probe loop.
                    std::vector<const RightItem*> posting_rows(keyed.size());
                    for (const RowRef& entry : keyed)
                        posting_rows[counts[bucket(entry.key)]++] = entry.row;
                    for (const auto& l : left) {
                        const Key k = left_key(l);
                        if (k < min_key || k > max_key) continue;
                        const auto slot = bucket(k);
                        for (std::size_t p = offsets[slot]; p < offsets[slot + 1]; ++p)
                            visitor(l, *posting_rows[p]);
                    }
                    return;
                }
            }
            // Sparse/large-range integral input: reuse the cached projections,
            // avoiding a second call to the user-supplied right_key callback.
            std::unordered_multimap<Key, const RightItem*> index;
            index.reserve(keyed.size());
            for (const RowRef& entry : keyed) index.emplace(entry.key, entry.row);
            for (const auto& l : left) {
                auto [begin, end] = index.equal_range(left_key(l));
                for (; begin != end; ++begin) visitor(l, *begin->second);
            }
            return;
        }
    }

    std::unordered_multimap<Key, const RightItem*> index;
    for (const auto& r : right) index.emplace(right_key(r), &r);
    for (const auto& l : left) {
        auto [first, last] = index.equal_range(left_key(l));
        for (; first != last; ++first) visitor(l, *first->second);
    }
}
} // namespace newton::analytics
