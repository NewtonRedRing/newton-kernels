#include <newton/index/inverted_index.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
namespace newton::index {
namespace {
bool word_char(unsigned char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }
std::vector<std::string> tokens(std::string_view s) {
    std::vector<std::string> out;
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
std::vector<InvertedIndex::DocumentId> InvertedIndex::search_all(std::string_view text) const {
    const auto words = tokens(text);
    if (words.empty()) return {};
    std::vector<DocumentId> hits;
    for (std::size_t i = 0; i < words.size(); ++i) {
        const auto it = terms_.find(words[i]);
        if (it == terms_.end()) return {};
        const auto& postings = it->second;
        if (i == 0) {
            hits.reserve(postings.size());
            for (const auto& p : postings) hits.push_back(p.id);
        } else {
            std::vector<DocumentId> next;
            std::size_t a = 0, b = 0;
            while (a < hits.size() && b < postings.size()) {
                if (hits[a] == postings[b].id) { next.push_back(hits[a]); ++a; ++b; }
                else if (hits[a] < postings[b].id) ++a;
                else ++b;
            }
            hits = std::move(next);
        }
        if (hits.empty()) return {};
    }
    return hits;
}
std::vector<InvertedIndex::DocumentId> InvertedIndex::search_phrase(std::string_view phrase) const {
    const auto words = tokens(phrase);
    if (words.empty()) return {};
    auto candidates = search_all(phrase);
    std::vector<DocumentId> result;
    for (DocumentId id : candidates) {
        const auto& first = terms_.at(words.front());
        const auto pos = std::lower_bound(first.begin(), first.end(), id,
                             [](const Posting& p, DocumentId x) { return p.id < x; });
        for (std::uint32_t start : pos->positions) {
            bool matching = true;
            for (std::size_t i = 1; i < words.size(); ++i) {
                if (i > std::numeric_limits<std::uint32_t>::max() - start) { matching = false; break; }
                const auto& entries = terms_.at(words[i]);
                const auto found = std::lower_bound(entries.begin(), entries.end(), id,
                                     [](const Posting& p, DocumentId x) { return p.id < x; });
                if (found == entries.end() || found->id != id ||
                    !std::binary_search(found->positions.begin(), found->positions.end(), start + static_cast<std::uint32_t>(i))) {
                    matching = false;
                    break;
                }
            }
            if (matching) { result.push_back(id); break; }
        }
    }
    return result;
}
} // namespace newton::index
