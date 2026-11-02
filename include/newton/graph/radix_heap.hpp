#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
namespace newton::graph {
// Monotone unsigned integer priority queue; keys pushed must be >= last popped.
class RadixHeap {
public:
    using Entry = std::pair<std::uint64_t, std::uint32_t>;
    void push(std::uint64_t key, std::uint32_t value) {
        if (key < last_) throw std::invalid_argument("RadixHeap requires monotone keys");
        buckets_[bucket(key)].emplace_back(key, value);
        ++size_;
    }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    Entry pop() {
        if (empty()) throw std::out_of_range("RadixHeap is empty");
        if (buckets_[0].empty()) {
            std::size_t b = 1;
            while (b < buckets_.size() && buckets_[b].empty()) ++b;
            auto min_key = std::numeric_limits<std::uint64_t>::max();
            for (const auto& e : buckets_[b]) if (e.first < min_key) min_key = e.first;
            last_ = min_key;
            auto entries = std::move(buckets_[b]);
            buckets_[b].clear();
            for (auto e : entries) buckets_[bucket(e.first)].push_back(e);
        }
        auto entry = buckets_[0].back();
        buckets_[0].pop_back();
        --size_;
        return entry;
    }
private:
    [[nodiscard]] std::size_t bucket(std::uint64_t key) const noexcept {
        return static_cast<std::size_t>(64 - std::countl_zero(key ^ last_));
    }
    std::array<std::vector<Entry>, 65> buckets_{};
    std::uint64_t last_ = 0;
    std::size_t size_ = 0;
};
} // namespace newton::graph
