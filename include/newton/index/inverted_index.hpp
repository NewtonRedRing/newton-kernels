#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace newton::index {
// Simple in-memory ASCII-alphanumeric positional inverted index.
// Document identifiers are allocated monotonically from zero.
class InvertedIndex {
public:
    using DocumentId = std::uint32_t;
    DocumentId add_document(std::string_view text);
    [[nodiscard]] std::vector<DocumentId> search_all(std::string_view terms) const;
    [[nodiscard]] std::vector<DocumentId> search_phrase(std::string_view phrase) const;
    [[nodiscard]] std::size_t document_count() const noexcept { return documents_; }
private:
    struct Posting { DocumentId id; std::vector<std::uint32_t> positions; };
    std::unordered_map<std::string, std::vector<Posting>> terms_;
    std::size_t documents_ = 0;
};
} // namespace newton::index
