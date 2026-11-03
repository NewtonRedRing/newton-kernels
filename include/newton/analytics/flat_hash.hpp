#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace newton::analytics {
// Independent exact-key open-addressed accumulator. Allocate once when
// constructed; no allocations during add/find/merge while within capacity.
// Full 64-bit equality resolves collisions; hash fingerprints alone do not.
class FlatCountSum {
public:
    struct Result { std::uint64_t count = 0, sum = 0; };
    explicit FlatCountSum(std::size_t max_distinct) {
        if (max_distinct > (std::numeric_limits<std::size_t>::max() / 2) - 1)
            throw std::length_error("FlatCountSum capacity overflow");
        std::size_t slots = 2;
        while (slots < max_distinct * 2 + 1) {
            if (slots > std::numeric_limits<std::size_t>::max() / 2)
                throw std::length_error("FlatCountSum slot overflow");
            slots *= 2;
        }
        slots_.resize(slots);
    }
    void clear() noexcept {
        for (auto& slot : slots_) slot.used = false;
        count_ = 0;
    }
    void add(std::uint64_t key, std::uint64_t value) {
        auto pos = static_cast<std::size_t>(mix(key)) & (slots_.size() - 1);
        for (std::size_t tries = 0; tries < slots_.size(); ++tries) {
            auto& slot = slots_[pos];
            if (!slot.used) {
                slot = Slot{key, 1, value, true};
                ++count_;
                return;
            }
            if (slot.key == key) {
                if (slot.count == std::numeric_limits<std::uint64_t>::max() ||
                    value > std::numeric_limits<std::uint64_t>::max() - slot.sum)
                    throw std::overflow_error("FlatCountSum accumulator overflow");
                ++slot.count;
                slot.sum += value;
                return;
            }
            pos = (pos + 1) & (slots_.size() - 1);
        }
        throw std::length_error("FlatCountSum table is full");
    }
    [[nodiscard]] std::optional<Result> find(std::uint64_t key) const noexcept {
        auto pos = static_cast<std::size_t>(mix(key)) & (slots_.size() - 1);
        for (std::size_t tries = 0; tries < slots_.size(); ++tries) {
            const auto& slot = slots_[pos];
            if (!slot.used) return std::nullopt;
            if (slot.key == key) return Result{slot.count, slot.sum};
            pos = (pos + 1) & (slots_.size() - 1);
        }
        return std::nullopt;
    }
    template<class Visitor>
    void for_each(Visitor&& visitor) const {
        for (const auto& slot : slots_)
            if (slot.used) visitor(slot.key, Result{slot.count, slot.sum});
    }
    [[nodiscard]] std::size_t distinct() const noexcept { return count_; }
    static constexpr std::uint64_t mix(std::uint64_t x) noexcept {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }
private:
    struct Slot {
        std::uint64_t key = 0;
        std::uint64_t count = 0;
        std::uint64_t sum = 0;
        bool used = false;
    };
    std::vector<Slot> slots_;
    std::size_t count_ = 0;
};

// Allocator-free duplicate-preserving hash join, with caller-owned chain
// arrays. heads must have at least one slot; next.size >= right.size.
// Existing head contents are overwritten. Exact key checks handle collisions.
// Visitor receives original left/right row indices. No output ordering promise.
template<class Visitor>
std::size_t flat_hash_join_into(
    std::span<const std::uint64_t> left,
    std::span<const std::uint64_t> right,
    std::span<std::size_t> heads,
    std::span<std::size_t> next,
    Visitor&& visitor) {
    if (heads.empty() || next.size() < right.size())
        throw std::length_error("Insufficient caller-owned join scratch");
    constexpr auto missing = std::numeric_limits<std::size_t>::max();
    std::fill(heads.begin(), heads.end(), missing);
    for (std::size_t i = 0; i < right.size(); ++i) {
        const auto bucket = static_cast<std::size_t>(FlatCountSum::mix(right[i]) % heads.size());
        next[i] = heads[bucket];
        heads[bucket] = i;
    }
    std::size_t count = 0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto bucket = static_cast<std::size_t>(FlatCountSum::mix(left[i]) % heads.size());
        for (auto k = heads[bucket]; k != missing; k = next[k]) {
            if (left[i] != right[k]) continue;
            if (count == missing) throw std::overflow_error("Join match count overflow");
            visitor(i, k);
            ++count;
        }
    }
    return count;
}
} // namespace newton::analytics
