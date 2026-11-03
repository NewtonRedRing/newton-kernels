#pragma once
#include <newton/columnar/views.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace newton::analytics {

// No allocations; filters null predicates out. Output capacity must be at
// least input size (so failure cannot partially write). Indices are u32.
template<class T>
std::size_t select_greater_into(
        newton::columnar::ColumnView<T> predicates, T threshold,
        std::span<std::uint32_t> output) {
    if (predicates.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Selection exceeds 32-bit row IDs");
    if (output.size() < predicates.size())
        throw std::length_error("Selection output requires row_count capacity");
    std::size_t count = 0;
    for (std::size_t row = 0; row < predicates.size(); ++row)
        if (predicates.valid(row) && predicates.value(row) > threshold)
            output[count++] = static_cast<std::uint32_t>(row);
    return count;
}

struct SumResult {
    std::uint64_t count = 0;
    std::uint64_t sum = 0;
};

// Fuses predicate+null checks+unsigned sum in a single pass. A row only
// contributes when BOTH predicate and value are valid. No output arrays.
inline SumResult sum_where_greater(
        newton::columnar::ColumnView<std::int64_t> predicate,
        newton::columnar::ColumnView<std::uint64_t> values,
        std::int64_t threshold) {
    if (predicate.size() != values.size())
        throw std::invalid_argument("Predicate and value column lengths differ");
    SumResult out;
    for (std::size_t i = 0; i < predicate.size(); ++i) {
        if (!predicate.valid(i) || !values.valid(i) ||
            predicate.value(i) <= threshold) continue;
        if (out.count == std::numeric_limits<std::uint64_t>::max() ||
            values.value(i) > std::numeric_limits<std::uint64_t>::max() - out.sum)
            throw std::overflow_error("Fused columnar sum overflow");
        ++out.count;
        out.sum += values.value(i);
    }
    return out;
}

} // namespace newton::analytics
