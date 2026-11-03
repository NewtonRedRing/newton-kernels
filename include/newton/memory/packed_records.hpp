#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace newton::memory {
// A full-range, reversible u64 -> u32 compact representation. The top u32
// code is an escape and its true value lives in a caller-owned side buffer.
// No allocations; both arrays stay owned by the caller.
struct PackedException {
    std::size_t index;
    std::uint64_t value;
};
inline std::size_t pack_uint64_into(
    std::span<const std::uint64_t> input,
    std::span<std::uint32_t> codes,
    std::span<PackedException> exceptions) {
    if (codes.size() < input.size())
        throw std::length_error("Packed record codes capacity is too small");
    constexpr auto sentinel = std::numeric_limits<std::uint32_t>::max();
    std::size_t needed = 0;
    for (const auto value : input) needed += value >= sentinel;
    if (exceptions.size() < needed)
        throw std::length_error("Packed exception capacity is too small");
    std::size_t used = 0;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] >= sentinel) {
            codes[i] = sentinel;
            exceptions[used++] = {i, input[i]};
        } else {
            codes[i] = static_cast<std::uint32_t>(input[i]);
        }
    }
    return used;
}
inline std::uint64_t unpack_uint64_at(
    std::span<const std::uint32_t> codes,
    std::span<const PackedException> exceptions,
    std::size_t index) {
    if (index >= codes.size()) throw std::out_of_range("Packed record index");
    constexpr auto sentinel = std::numeric_limits<std::uint32_t>::max();
    if (codes[index] != sentinel) return codes[index];
    const auto it = std::lower_bound(exceptions.begin(), exceptions.end(), index,
        [](const PackedException& e, std::size_t i) { return e.index < i; });
    if (it == exceptions.end() || it->index != index)
        throw std::invalid_argument("Missing packed-record exception");
    return it->value;
}
} // namespace newton::memory
