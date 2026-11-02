#pragma once
#include <cstddef>
#include <string_view>

namespace newton::simd {
// Auto chooses the best implementation supported by the current CPU.
// Explicit strategies permit reproducible benchmarks and tuning; unsupported
// SIMD strategies safely fall back to Scalar.
enum class ByteScanStrategy { Auto, Scalar, Avx2, Avx512 };
[[nodiscard]] bool supports_byte_scan(ByteScanStrategy strategy) noexcept;
// Both functions accept arbitrary binary data, including embedded NULs.
// A missing needle produces input.size() for find_byte.
[[nodiscard]] std::size_t find_byte(std::string_view input, char needle) noexcept;
[[nodiscard]] std::size_t count_byte(std::string_view input, char needle) noexcept;
[[nodiscard]] std::size_t find_byte_using(
    std::string_view input, char needle, ByteScanStrategy strategy) noexcept;
[[nodiscard]] std::size_t count_byte_using(
    std::string_view input, char needle, ByteScanStrategy strategy) noexcept;
} // namespace newton::simd
