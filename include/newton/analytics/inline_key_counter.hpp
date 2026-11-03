#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace newton::analytics {

// Flat, full-key-exact string counter. Short keys stay inline in each slot;
// long keys fall back to owned std::string storage. Keys are arbitrary bytes.
// Instance can be owned independently by each caller/thread, then merged.
// No global mutable table, no background tasks, no hash-only equality.
class InlineKeyCounter {
public:
    static constexpr std::size_t inline_capacity = 24;
    struct Result { std::uint64_t count = 0, sum = 0; };
    explicit InlineKeyCounter(std::size_t expected_keys) {
        if (expected_keys > (std::numeric_limits<std::size_t>::max() - 1) / 2)
            throw std::length_error("InlineKeyCounter capacity overflow");
        std::size_t n = 2;
        while (n < expected_keys * 2 + 1) {
            if (n > std::numeric_limits<std::size_t>::max() / 2)
                throw std::length_error("InlineKeyCounter slots overflow");
            n *= 2;
        }
        slots_.resize(n);
    }
    void clear() noexcept {
        for (auto& s : slots_) {
            s.used = false;
            s.long_key.clear();
        }
        distinct_ = 0;
    }
    void add(std::string_view key, std::uint64_t value) { add_partial(key, 1, value); }
    void add_partial(std::string_view key, std::uint64_t count, std::uint64_t sum) {
        if (count == 0) {
            if (sum != 0) throw std::invalid_argument("Zero count with nonzero sum");
            return;
        }
        auto pos = static_cast<std::size_t>(hash(key) & (slots_.size() - 1));
        for (std::size_t probes = 0; probes < slots_.size(); ++probes) {
            auto& s = slots_[pos];
            if (!s.used) {
                s.used = true;
                s.length = key.size();
                s.count = count;
                s.sum = sum;
                if (key.size() <= inline_capacity) {
                    for (std::size_t i = 0; i < key.size(); ++i) s.inline_key[i] = key[i];
                    s.long_key.clear();
                } else {
                    try { s.long_key.assign(key); }
                    catch (...) { s.used = false; throw; }
                }
                ++distinct_;
                return;
            }
            if (s.length == key.size() && s.key_view() == key) {
                if (count > std::numeric_limits<std::uint64_t>::max() - s.count ||
                    sum > std::numeric_limits<std::uint64_t>::max() - s.sum)
                    throw std::overflow_error("InlineKeyCounter accumulator overflow");
                s.count += count;
                s.sum += sum;
                return;
            }
            pos = (pos + 1) & (slots_.size() - 1);
        }
        throw std::length_error("InlineKeyCounter table full");
    }
    Result find(std::string_view key) const noexcept {
        auto pos = static_cast<std::size_t>(hash(key) & (slots_.size() - 1));
        for (std::size_t probes = 0; probes < slots_.size(); ++probes) {
            const auto& s = slots_[pos];
            if (!s.used) return {};
            if (s.length == key.size() && s.key_view() == key) return {s.count, s.sum};
            pos = (pos + 1) & (slots_.size() - 1);
        }
        return {};
    }
    template<class Visitor>
    void for_each(Visitor&& visitor) const {
        for (const auto& s : slots_) if (s.used)
            visitor(s.key_view(), Result{s.count, s.sum});
    }
    void merge_from(const InlineKeyCounter& other) {
        other.for_each([&](std::string_view key, Result item) {
            add_partial(key, item.count, item.sum);
        });
    }
    std::size_t distinct() const noexcept { return distinct_; }
private:
    struct Slot {
        std::array<char, inline_capacity> inline_key{};
        std::string long_key;
        std::size_t length = 0;
        std::uint64_t count = 0, sum = 0;
        bool used = false;
        std::string_view key_view() const noexcept {
            return length <= inline_capacity ?
                std::string_view(inline_key.data(), length) :
                std::string_view(long_key);
        }
    };
    static std::uint64_t hash(std::string_view text) noexcept {
        std::uint64_t h = 14695981039346656037ULL;
        for (unsigned char c : text) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        return h;
    }
    std::vector<Slot> slots_;
    std::size_t distinct_ = 0;
};
} // namespace newton::analytics
