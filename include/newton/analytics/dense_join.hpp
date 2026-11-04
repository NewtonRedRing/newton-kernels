#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace newton::analytics {

// Exact duplicate-preserving join for a bounded, contiguous uint64 key domain.
// heads indexes CSR-style contiguous postings for keys in the bounded domain.
// next.size() must be >= right_keys.size(); both scratch buffers are caller-owned.
// Inputs must not alias scratch. Visitor receives original left/right indices.
// Right keys outside the domain fail before any scratch or visitor changes;
// left keys outside the domain have no matches. Match order is not specified.
// Time: O(domain size + left size + right size + matching pairs).
// Two right-side passes yield cache-local, stable posting order without hashing.
// Extra memory: heads.size() + right_keys.size() elements, supplied by caller.
// For sparse/wide domains, use flat_hash_join_into instead.
template <class Visitor>
std::size_t dense_join_into(std::span<const std::uint64_t> left_keys,
                            std::span<const std::uint64_t> right_keys,
                            std::uint64_t lower_key,
                            std::span<std::size_t> heads,
                            std::span<std::size_t> next,
                            Visitor&& visitor) {
    if (heads.empty() || next.size() < right_keys.size())
        throw std::length_error("Insufficient caller-owned dense join scratch");

    // Subtraction only after key >= lower_key prevents integer wraparound,
    // including when lower_key is near UINT64_MAX.
    for (std::uint64_t key : right_keys)
        if (key < lower_key || key - lower_key >= heads.size())
            throw std::out_of_range("Right join key outside dense domain");

    // Histogram exact keys into a compact CSR posting layout, without one
    // heap allocation per distinct key and without pointer chasing per match.
    std::fill(heads.begin(), heads.end(), 0);
    for (const auto key : right_keys)
        ++heads[static_cast<std::size_t>(key - lower_key)];

    std::size_t end = 0;
    for (auto& bucket : heads) {
        const auto count = bucket;
        end += count;  // Never exceeds right_keys.size().
        bucket = end;  // Exclusive end for stable counting-sort scatter.
    }
    // Reverse iteration + decrementing cursors preserves original right order.
    for (std::size_t i = right_keys.size(); i-- > 0;) {
        auto& end_cursor = heads[static_cast<std::size_t>(right_keys[i] - lower_key)];
        next[--end_cursor] = i;
    }

    std::size_t matches = 0;
    for (std::size_t i = 0; i < left_keys.size(); ++i) {
        const auto key = left_keys[i];
        if (key < lower_key || key - lower_key >= heads.size()) continue;
        const auto slot = static_cast<std::size_t>(key - lower_key);
        const auto end_cursor = slot + 1 < heads.size() ? heads[slot + 1] : right_keys.size();
        for (auto cursor = heads[slot]; cursor < end_cursor; ++cursor) {
            if (matches == std::numeric_limits<std::size_t>::max())
                throw std::overflow_error("Join match count overflow");
            visitor(i, next[cursor]);
            ++matches;
        }
    }
    return matches;
}
} // namespace newton::analytics
