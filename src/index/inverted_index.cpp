#include <newton/index/inverted_index.hpp>
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>
namespace newton::index {
namespace {
bool word_char(unsigned char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
std::vector<std::string> tokens(std::string_view s) {
    std::vector<std::string> out;
    // A capped token-count hint amortizes vector growth for common short
    // documents without preallocating in proportion to huge input buffers.
    // The exact tokenization semantics and allocation-failure behavior remain
    // unchanged: this only reserves storage before parsing any content.
    if (!s.empty())
        out.reserve(std::min<std::size_t>(64, s.size() / 6 + 1));
    std::string token;
    for (unsigned char c : s) {
        if (word_char(c)) token.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c));
        else if (!token.empty()) { out.push_back(std::move(token)); token.clear(); }
    }
    if (!token.empty()) out.push_back(std::move(token));
    return out;
}
} // namespace
InvertedIndex::DocumentId InvertedIndex::add_document(std::string_view text) {
    if (documents_ >= std::numeric_limits<DocumentId>::max()) throw std::length_error("Too many documents");
    auto words = tokens(text);
    if (words.size() > std::numeric_limits<std::uint32_t>::max()) throw std::length_error("Document too long");
    const auto id = static_cast<DocumentId>(documents_++);
    for (std::uint32_t i = 0; i < words.size(); ++i) {
        auto& postings = terms_[words[i]];
        if (postings.empty() || postings.back().id != id) postings.push_back({id, {}});
        postings.back().positions.push_back(i);
    }
    return id;
}
std::vector<InvertedIndex::DocumentId> InvertedIndex::search_all(
    std::string_view text, IntersectionStrategy strategy) const {
    const auto words = tokens(text);
    if (words.empty()) return {};
    std::vector<const std::vector<Posting>*> lists;
    lists.reserve(words.size());
    for (const auto& word : words) {
        auto it = terms_.find(word);
        if (it == terms_.end()) return {};
        lists.push_back(&it->second);
    }
    if (strategy == IntersectionStrategy::SmallestFirst) {
        std::sort(lists.begin(), lists.end(), [](auto* a, auto* b) { return a->size() < b->size(); });
    }
    std::vector<DocumentId> hits;
    hits.reserve(lists.front()->size());
    for (const auto& p : *lists.front()) hits.push_back(p.id);
    // In-place intersection avoids intermediate allocations.
    for (std::size_t term = 1; term < lists.size() && !hits.empty(); ++term) {
        const auto& postings = *lists[term];
        std::size_t a = 0, b = 0, out = 0;
        while (a < hits.size() && b < postings.size()) {
            if (hits[a] == postings[b].id) { hits[out++] = hits[a++]; ++b; }
            else if (hits[a] < postings[b].id) ++a;
            else ++b;
        }
        hits.resize(out);
    }
    return hits;
}
std::vector<InvertedIndex::DocumentId> InvertedIndex::search_phrase(std::string_view phrase) const {
    const auto words = tokens(phrase);
    if (words.empty()) return {};
    std::vector<const std::vector<Posting>*> lists;
    lists.reserve(words.size());
    for (const auto& word : words) {
        auto it = terms_.find(word);
        if (it == terms_.end()) return {};
        lists.push_back(&it->second);
    }
    std::size_t anchor = 0;
    for (std::size_t i = 1; i < lists.size(); ++i)
        if (lists[i]->size() < lists[anchor]->size()) anchor = i;
    std::vector<DocumentId> matches;
    // One pointer table per query, not one heap allocation per candidate
    // document. Every slot is refreshed before it is read for a match.
    // Common short phrases use stack scratch; longer phrases retain a single
    // allocation per query rather than one per candidate document.
    std::array<const Posting*, 8> small_doc{};
    std::vector<const Posting*> large_doc;
    const Posting** same_doc = small_doc.data();
    if (words.size() > small_doc.size()) {
        large_doc.resize(words.size());
        same_doc = large_doc.data();
    }
    for (const Posting& anchor_post : *lists[anchor]) {
        const DocumentId doc = anchor_post.id;
        // One binary search per term/doc, not per candidate position.
        same_doc[anchor] = &anchor_post;
        bool possible = true;
        for (std::size_t i = 0; i < lists.size(); ++i) {
            if (i == anchor) continue;
            const auto& entries = *lists[i];
            const auto it = std::lower_bound(entries.begin(), entries.end(), doc,
                [](const Posting& p, DocumentId x) { return p.id < x; });
            if (it == entries.end() || it->id != doc) { possible = false; break; }
            same_doc[i] = &*it;
        }
        if (!possible) continue;
        for (const auto position : anchor_post.positions) {
            if (position < anchor) continue;
            const auto start = position - static_cast<std::uint32_t>(anchor);
            if (words.size() - 1 > std::numeric_limits<std::uint32_t>::max() - start) continue;
            bool matching = true;
            for (std::size_t i = 0; i < lists.size(); ++i) {
                if (i == anchor) continue;
                if (!std::binary_search(same_doc[i]->positions.begin(), same_doc[i]->positions.end(),
                                        start + static_cast<std::uint32_t>(i))) { matching = false; break; }
            }
            if (matching) { matches.push_back(doc); break; }
        }
    }
    return matches;
}
} // namespace newton::index
