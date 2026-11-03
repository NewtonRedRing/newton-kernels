#pragma once
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace newton::index {
// Immutable exact postings: bitmap for dense terms, 16-bit offsets within
// 64K-document segments otherwise. No hash-only equality assumptions.
class HybridPostings {
public:
    enum class Format { Segmented16, Bitmap };
    struct Segment { std::uint16_t high; std::vector<std::uint16_t> lows; };
    static HybridPostings from_sorted(
        std::span<const std::uint32_t> docs, std::uint32_t universe) {
        HybridPostings result;
        result.universe_ = universe;
        result.count_ = docs.size();
        for (std::size_t i = 0; i < docs.size(); ++i)
            if (docs[i] >= universe || (i && docs[i] <= docs[i - 1]))
                throw std::invalid_argument("Posting IDs must be sorted, unique and within domain");
        const auto words = (static_cast<std::size_t>(universe) + 63) / 64;
        if (!docs.empty() && docs.size() * 4 >= words * 8) {
            result.format_ = Format::Bitmap;
            result.bits_.assign(words, 0);
            for (const auto id : docs) result.bits_[id / 64] |= std::uint64_t{1} << (id % 64);
        } else {
            for (const auto id : docs) {
                const auto high = static_cast<std::uint16_t>(id >> 16);
                if (result.segments_.empty() || result.segments_.back().high != high)
                    result.segments_.push_back({high, {}});
                result.segments_.back().lows.push_back(static_cast<std::uint16_t>(id));
            }
        }
        return result;
    }
    bool contains(std::uint32_t id) const noexcept {
        if (id >= universe_) return false;
        if (format_ == Format::Bitmap)
            return (bits_[id / 64] & (std::uint64_t{1} << (id % 64))) != 0;
        const auto high = static_cast<std::uint16_t>(id >> 16);
        const auto found = std::lower_bound(segments_.begin(), segments_.end(), high,
            [](const Segment& seg, std::uint16_t k) { return seg.high < k; });
        return found != segments_.end() && found->high == high &&
               std::binary_search(found->lows.begin(), found->lows.end(),
                                  static_cast<std::uint16_t>(id));
    }
    template<class Visitor>
    void for_each(Visitor&& visit) const {
        if (format_ == Format::Bitmap) {
            for (std::size_t i = 0; i < bits_.size(); ++i) {
                auto word = bits_[i];
                while (word) {
                    visit(static_cast<std::uint32_t>(i * 64 + std::countr_zero(word)));
                    word &= word - 1;
                }
            }
        } else {
            for (const auto& seg : segments_)
                for (const auto low : seg.lows)
                    visit((static_cast<std::uint32_t>(seg.high) << 16) | low);
        }
    }
    Format format() const noexcept { return format_; }
    std::size_t size() const noexcept { return count_; }
private:
    Format format_ = Format::Segmented16;
    std::uint32_t universe_ = 0;
    std::size_t count_ = 0;
    std::vector<std::uint64_t> bits_;
    std::vector<Segment> segments_;
};

// Galloping intersection of strictly increasing ID spans, no allocations.
inline std::size_t intersect_galloping_into(
    std::span<const std::uint32_t> a,
    std::span<const std::uint32_t> b,
    std::span<std::uint32_t> output) {
    if (output.size() < std::min(a.size(), b.size()))
        throw std::length_error("Intersection output too small");
    if (a.size() > b.size()) std::swap(a, b);
    std::size_t cursor = 0, count = 0;
    for (const auto id : a) {
        if (cursor == b.size()) break;
        const auto base = cursor;
        std::size_t step = 1;
        while (step < b.size() - base && b[base + step] < id) {
            if (step > (b.size() - base) / 2) { step = b.size() - base; break; }
            step *= 2;
        }
        const auto end = base + std::min(step + (step < b.size() - base), b.size() - base);
        const auto found = std::lower_bound(
            b.begin() + static_cast<std::ptrdiff_t>(base),
            b.begin() + static_cast<std::ptrdiff_t>(end), id);
        cursor = static_cast<std::size_t>(found - b.begin());
        if (cursor < b.size() && b[cursor] == id) {
            output[count++] = id;
            ++cursor;
        }
    }
    return count;
}

// Index only terms requested BEFORE documents are scanned. Supports Boolean
// AND on exact case-folded ASCII tokens. Phrase search remains in InvertedIndex.
class SelectiveTermIndex {
public:
    void add_query(std::string_view query) {
        if (documents_ || finalized_) throw std::logic_error("Queries must be declared first");
        tokenize(query, [&](const std::string& token) {
            if (!lookup_.contains(token)) {
                lookup_.emplace(token, raw_.size());
                raw_.emplace_back();
            }
        });
    }
    std::uint32_t add_document(std::string_view document) {
        if (finalized_) throw std::logic_error("Index already finalized");
        if (documents_ == std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Document ID overflow");
        const auto id = documents_++;
        tokenize(document, [&](const std::string& token) {
            const auto it = lookup_.find(token);
            if (it == lookup_.end()) return;
            auto& ids = raw_[it->second];
            if (ids.empty() || ids.back() != id) ids.push_back(id);
        });
        return id;
    }
    void finalize() {
        if (finalized_) return;
        postings_.reserve(raw_.size());
        for (const auto& ids : raw_)
            postings_.push_back(HybridPostings::from_sorted(ids, documents_));
        raw_.clear();
        raw_.shrink_to_fit();
        finalized_ = true;
    }
    std::size_t search_all_into(std::string_view query,
                                std::span<std::uint32_t> output) const {
        if (!finalized_) throw std::logic_error("Index not finalized");
        std::vector<std::size_t> terms;
        bool unknown = false;
        tokenize(query, [&](const std::string& token) {
            const auto it = lookup_.find(token);
            if (it == lookup_.end()) unknown = true;
            else terms.push_back(it->second);
        });
        if (terms.empty() || unknown) return 0;
        std::sort(terms.begin(), terms.end(),
            [&](auto a, auto b) { return postings_[a].size() < postings_[b].size(); });
        std::size_t count = 0;
        postings_[terms.front()].for_each([&](std::uint32_t id) {
            for (std::size_t i = 1; i < terms.size(); ++i)
                if (!postings_[terms[i]].contains(id)) return;
            if (count == output.size()) throw std::length_error("Query output too small");
            output[count++] = id;
        });
        return count;
    }
    std::size_t document_count() const noexcept { return documents_; }
    std::size_t indexed_term_count() const noexcept { return lookup_.size(); }
private:
    template<class Visitor>
    static void tokenize(std::string_view text, Visitor&& visit) {
        std::string token;
        for (unsigned char c : text) {
            const bool word = (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
            if (word) token.push_back(static_cast<char>(
                c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
            else if (!token.empty()) {
                visit(token);
                token.clear();
            }
        }
        if (!token.empty()) visit(token);
    }
    std::unordered_map<std::string, std::size_t> lookup_;
    std::vector<std::vector<std::uint32_t>> raw_;
    std::vector<HybridPostings> postings_;
    std::uint32_t documents_ = 0;
    bool finalized_ = false;
};
} // namespace newton::index
