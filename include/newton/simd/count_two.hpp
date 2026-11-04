#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define NEWTON_DUAL_SCAN_X86 1
#endif

namespace newton::simd {
namespace dual_scan_detail {
using CountFn = std::size_t (*)(std::string_view, char, char) noexcept;

inline std::size_t scalar(std::string_view s, char a, char b) noexcept {
    std::size_t count = 0;
    for (char ch : s) count += (ch == a || ch == b);
    return count;
}

#ifdef NEWTON_DUAL_SCAN_X86
__attribute__((target("avx2"))) inline std::size_t avx2(
    std::string_view s, char a, char b) noexcept {
    const auto* bytes = s.data();
    const auto x = _mm256_set1_epi8(a);
    const auto y = _mm256_set1_epi8(b);
    std::size_t pos = 0, count = 0;
    for (; s.size() - pos >= 32; pos += 32) {
        const auto block = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(bytes + pos));
        const auto eq = _mm256_or_si256(_mm256_cmpeq_epi8(block, x),
                                       _mm256_cmpeq_epi8(block, y));
        count += std::popcount(static_cast<std::uint32_t>(
            _mm256_movemask_epi8(eq)));
    }
    for (; pos < s.size(); ++pos)
        count += (bytes[pos] == a || bytes[pos] == b);
    return count;
}

__attribute__((target("avx512f,avx512bw"))) inline std::size_t avx512(
    std::string_view s, char a, char b) noexcept {
    const auto* bytes = s.data();
    const auto x = _mm512_set1_epi8(a);
    const auto y = _mm512_set1_epi8(b);
    std::size_t pos = 0, count = 0;
    for (; s.size() - pos >= 64; pos += 64) {
        const auto block = _mm512_loadu_si512(static_cast<const void*>(bytes + pos));
        const std::uint64_t masks =
            static_cast<std::uint64_t>(_mm512_cmpeq_epi8_mask(block, x)) |
            static_cast<std::uint64_t>(_mm512_cmpeq_epi8_mask(block, y));
        count += std::popcount(masks);
    }
    for (; pos < s.size(); ++pos)
        count += (bytes[pos] == a || bytes[pos] == b);
    return count;
}

inline CountFn select() noexcept {
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw"))
        return avx512;
    if (__builtin_cpu_supports("avx2")) return avx2;
    return scalar;
}
#else
inline CountFn select() noexcept { return scalar; }
#endif
} // namespace dual_scan_detail

// Count bytes matching either delimiter exactly once (including a == b).
// Binary-safe, allocation-free, runtime ISA-dispatched with portable fallback.
// A single pass can replace two independent scans when counting delimiters
// collectively, but cannot supply *separate* per-delimiter counts.
[[nodiscard]] inline std::size_t count_either_byte(
    std::string_view input, char a, char b) noexcept {
    static const auto impl = dual_scan_detail::select();
    return impl(input, a, b);
}
} // namespace newton::simd
