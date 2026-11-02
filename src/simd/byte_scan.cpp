#include <newton/simd/byte_scan.hpp>
#include <newton/core/cpu_features.hpp>
#include <bit>
#include <cstdint>
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#  include <immintrin.h>
#  define NEWTON_X86_GNU 1
#endif
namespace newton::simd {
namespace {
std::size_t scalar_find(std::string_view s, char n) noexcept {
    const auto pos = s.find(n);
    return pos == std::string_view::npos ? s.size() : pos;
}
std::size_t scalar_count(std::string_view s, char n) noexcept {
    std::size_t total = 0;
    for (char c : s) total += (c == n);
    return total;
}
#ifdef NEWTON_X86_GNU
__attribute__((target("avx2"))) std::size_t avx2_find(std::string_view s, char n) noexcept {
    const auto* p = reinterpret_cast<const char*>(s.data());
    const auto needle = _mm256_set1_epi8(n);
    std::size_t i = 0;
    for (; s.size() - i >= 32; i += 32) {
        const auto data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p + i));
        const unsigned mask = static_cast<unsigned>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(data, needle)));
        if (mask) return i + std::countr_zero(mask);
    }
    for (; i < s.size(); ++i) if (p[i] == n) return i;
    return s.size();
}
__attribute__((target("avx2"))) std::size_t avx2_count(std::string_view s, char n) noexcept {
    const auto* p = reinterpret_cast<const char*>(s.data());
    const auto needle = _mm256_set1_epi8(n);
    std::size_t i = 0, total = 0;
    for (; s.size() - i >= 32; i += 32) {
        const auto data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p + i));
        const unsigned mask = static_cast<unsigned>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(data, needle)));
        total += std::popcount(mask);
    }
    for (; i < s.size(); ++i) total += (p[i] == n);
    return total;
}
#endif
} // namespace
std::size_t find_byte(std::string_view s, char n) noexcept {
#ifdef NEWTON_X86_GNU
    if (newton::core::cpu_features().avx2) return avx2_find(s, n);
#endif
    return scalar_find(s, n);
}
std::size_t count_byte(std::string_view s, char n) noexcept {
#ifdef NEWTON_X86_GNU
    if (newton::core::cpu_features().avx2) return avx2_count(s, n);
#endif
    return scalar_count(s, n);
}
} // namespace newton::simd
