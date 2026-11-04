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
    void clear() noexcept {
        for (auto& bucket : buckets_) bucket.clear();
        last_ = 0;
        size_ = 0;
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
            // Preserve the original bucket until redistribution commits.
            // In the exceptional path, undo only the elements appended by
            // this operation; no allocations are needed for the rollback.
            const auto old_last = last_;
            auto entries = std::move(buckets_[b]);
            last_ = min_key;
            std::size_t committed = 0;
            try {
                for (const auto& e : entries) {
                    buckets_[bucket(e.first)].push_back(e);
                    ++committed;
                }
            } catch (...) {
                // Keep correctness-critical rollback in a separate unlikely
                // routine. This also isolates EH unwind code from the hot
                // shortest-path instruction layout on supported compilers.
                rollback_rebucket(b, old_last, entries, committed);
                throw;
            }
        }
        auto entry = buckets_[0].back();
        buckets_[0].pop_back();
        --size_;
        return entry;
    }
private:
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((cold, noinline))
#elif defined(_MSC_VER)
    __declspec(noinline)
#endif
    void rollback_rebucket(std::size_t source_bucket,
                           std::uint64_t old_last,
                           std::vector<Entry>& entries,
                           std::size_t committed) noexcept {
        // Only successfully appended entries are undone; rollback never
        // allocates. Restore previous bucket and monotonic epoch afterwards.
        for (std::size_t i = 0; i < committed; ++i)
            buckets_[bucket(entries[i].first)].pop_back();
        last_ = old_last;
        buckets_[source_bucket] = std::move(entries);
    }
    [[nodiscard]] std::size_t bucket(std::uint64_t key) const noexcept {
        return static_cast<std::size_t>(64 - std::countl_zero(key ^ last_));
    }
    std::array<std::vector<Entry>, 65> buckets_{};
    std::uint64_t last_ = 0;
    std::size_t size_ = 0;
};
} // namespace newton::graph
