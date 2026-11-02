#pragma once
#include <cstddef>
#include <string_view>
namespace newton::simd {
// Returns input.size() when the needle is absent. Safe on empty and unaligned inputs.
[[nodiscard]] std::size_t find_byte(std::string_view input, char needle) noexcept;
[[nodiscard]] std::size_t count_byte(std::string_view input, char needle) noexcept;
} // namespace newton::simd
