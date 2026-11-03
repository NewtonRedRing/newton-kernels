#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define NEWTON_MULTI_X86 1
#endif

namespace newton::simd {
// Locate any of three delimiter bytes; returns input.size() when absent.
// AOT-compiled AVX2 path with an independent scalar fallback. No allocation.
namespace detail {
inline std::size_t find_any_scalar(std::string_view s, char a, char b, char c) noexcept {
    for (std::size_t i = 0; i < s.size(); ++i)
        if (s[i] == a || s[i] == b || s[i] == c) return i;
    return s.size();
}
#ifdef NEWTON_MULTI_X86
__attribute__((target("avx2"))) inline std::size_t find_any_avx2(
        std::string_view s, char a, char b, char c) noexcept {
    const auto va = _mm256_set1_epi8(a), vb = _mm256_set1_epi8(b), vc = _mm256_set1_epi8(c);
    std::size_t i = 0;
    for (; s.size() - i >= 32; i += 32) {
        const auto v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s.data() + i));
        const auto hit = _mm256_or_si256(_mm256_or_si256(_mm256_cmpeq_epi8(v, va),
                            _mm256_cmpeq_epi8(v, vb)), _mm256_cmpeq_epi8(v, vc));
        const auto mask = static_cast<std::uint32_t>(_mm256_movemask_epi8(hit));
        if (mask) return i + std::countr_zero(mask);
    }
    const auto suffix = s.substr(i);
    return i + find_any_scalar(suffix, a, b, c);
}
inline bool have_avx2() noexcept {
    static const bool enabled = []() noexcept {
        __builtin_cpu_init();
        return __builtin_cpu_supports("avx2") != 0;
    }();
    return enabled;
}
#endif
} // namespace detail

inline std::size_t find_any3(std::string_view data, char a, char b, char c) noexcept {
#ifdef NEWTON_MULTI_X86
    // Tiny inputs skip ISA selection and avoid an indirect dispatch.
    if (data.size() >= 64 && detail::have_avx2())
        return detail::find_any_avx2(data, a, b, c);
#endif
    return detail::find_any_scalar(data, a, b, c);
}
} // namespace newton::simd
