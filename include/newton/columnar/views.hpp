#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>

namespace newton::columnar {

// Immutable borrowed validity bitmap (Arrow-style: bit 1 means valid).
// Empty bitmap means all valid. Both storage and any views must outlive use.
struct ValidityView {
    std::span<const std::uint8_t> bytes{};
    std::size_t bit_offset = 0;

    void validate(std::size_t rows) const {
        if (bytes.empty()) return;
        if (bit_offset > std::numeric_limits<std::size_t>::max() - rows ||
            (bit_offset + rows) / 8 > bytes.size() ||
            ((bit_offset + rows) / 8 == bytes.size() && (bit_offset + rows) % 8 != 0))
            throw std::length_error("Validity bitmap does not cover all rows");
    }
    [[nodiscard]] bool valid(std::size_t row) const noexcept {
        if (bytes.empty()) return true;
        const auto bit = bit_offset + row;
        return ((bytes[bit / 8] >> (bit % 8)) & 1u) != 0;
    }
};

template<class T>
class ColumnView {
public:
    explicit ColumnView(std::span<const T> values, ValidityView validity = {})
        : values_(values), validity_(validity) { validity_.validate(values.size()); }
    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
    [[nodiscard]] bool valid(std::size_t row) const noexcept { return validity_.valid(row); }
    [[nodiscard]] const T& value(std::size_t row) const noexcept { return values_[row]; }
    [[nodiscard]] std::span<const T> values() const noexcept { return values_; }
    [[nodiscard]] ValidityView validity() const noexcept { return validity_; }
private:
    std::span<const T> values_;
    ValidityView validity_;
};

// Offset-based byte/string arrays. At creation, verify all boundaries to make
// lookups inexpensive. Encoding and collation are deliberately not inferred.
class StringColumnView {
public:
    StringColumnView(std::span<const std::int32_t> offsets,
                     std::string_view bytes, ValidityView validity = {})
        : offsets_(offsets), bytes_(bytes), validity_(validity) {
        if (offsets.empty()) throw std::invalid_argument("String offsets require a sentinel");
        const auto n = offsets.size() - 1;
        validity_.validate(n);
        if (offsets.front() < 0 || offsets.back() < 0 ||
            static_cast<std::size_t>(offsets.back()) > bytes.size())
            throw std::invalid_argument("String offsets exceed backing data");
        for (std::size_t i = 1; i < offsets.size(); ++i)
            if (offsets[i] < offsets[i - 1])
                throw std::invalid_argument("String offsets are not monotonic");
    }
    [[nodiscard]] std::size_t size() const noexcept { return offsets_.size() - 1; }
    [[nodiscard]] bool valid(std::size_t row) const noexcept { return validity_.valid(row); }
    [[nodiscard]] std::string_view value(std::size_t row) const noexcept {
        const auto begin = static_cast<std::size_t>(offsets_[row]);
        return bytes_.substr(begin,
            static_cast<std::size_t>(offsets_[row + 1] - offsets_[row]));
    }
private:
    std::span<const std::int32_t> offsets_;
    std::string_view bytes_;
    ValidityView validity_;
};

// Nonowning, type-erased batch metadata. No allocations, scheduler or context.
enum class ColumnKind { I32, U32, I64, U64, F32, F64, ByteString };
struct ColumnDescriptor {
    ColumnKind kind;
    const void* data;
    std::size_t length;
};
class RecordBatchView {
public:
    explicit RecordBatchView(std::span<const ColumnDescriptor> columns)
        : columns_(columns), rows_(columns.empty() ? 0 : columns.front().length) {
        for (const auto& column : columns)
            if (column.length != rows_ ||
                (rows_ && column.data == nullptr))
                throw std::invalid_argument("Inconsistent borrowed batch columns");
    }
    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] std::span<const ColumnDescriptor> columns() const noexcept {
        return columns_;
    }
private:
    std::span<const ColumnDescriptor> columns_;
    std::size_t rows_;
};

} // namespace newton::columnar
