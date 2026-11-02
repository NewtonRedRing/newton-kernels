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
using ScanFn = std::size_t (*)(std::string_view, char) noexcept;
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
    const auto* p = s.data();
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
    const auto* p = s.data();
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
__attribute__((target("avx512f,avx512bw"))) std::size_t avx512_find(std::string_view s, char n) noexcept {
    const auto* p = s.data();
    const auto needle = _mm512_set1_epi8(n);
    std::size_t i = 0;
    for (; s.size() - i >= 64; i += 64) {
        const auto data = _mm512_loadu_si512(static_cast<const void*>(p + i));
        const std::uint64_t mask = _mm512_cmpeq_epi8_mask(data, needle);
        if (mask) return i + std::countr_zero(mask);
    }
    for (; i < s.size(); ++i) if (p[i] == n) return i;
    return s.size();
}
__attribute__((target("avx512f,avx512bw"))) std::size_t avx512_count(std::string_view s, char n) noexcept {
    const auto* p = s.data();
    const auto needle = _mm512_set1_epi8(n);
    std::size_t i = 0, total = 0;
    for (; s.size() - i >= 64; i += 64) {
        const auto data = _mm512_loadu_si512(static_cast<const void*>(p + i));
        total += std::popcount(static_cast<std::uint64_t>(_mm512_cmpeq_epi8_mask(data, needle)));
    }
    for (; i < s.size(); ++i) total += (p[i] == n);
    return total;
}
#endif
ScanFn select_find([[maybe_unused]] ByteScanStrategy strategy) noexcept {
#ifdef NEWTON_X86_GNU
    const auto cpu = newton::core::cpu_features();
    if ((strategy == ByteScanStrategy::Auto || strategy == ByteScanStrategy::Avx512) &&
        cpu.avx512f && cpu.avx512bw) return avx512_find;
    if ((strategy == ByteScanStrategy::Auto || strategy == ByteScanStrategy::Avx2) &&
        cpu.avx2) return avx2_find;
#endif
    return scalar_find;
}
ScanFn select_count([[maybe_unused]] ByteScanStrategy strategy) noexcept {
#ifdef NEWTON_X86_GNU
    const auto cpu = newton::core::cpu_features();
    if ((strategy == ByteScanStrategy::Auto || strategy == ByteScanStrategy::Avx512) &&
        cpu.avx512f && cpu.avx512bw) return avx512_count;
    if ((strategy == ByteScanStrategy::Auto || strategy == ByteScanStrategy::Avx2) &&
        cpu.avx2) return avx2_count;
#endif
    return scalar_count;
}
} // namespace
bool supports_byte_scan(ByteScanStrategy strategy) noexcept {
    if (strategy == ByteScanStrategy::Auto || strategy == ByteScanStrategy::Scalar) return true;
#ifdef NEWTON_X86_GNU
    const auto cpu = newton::core::cpu_features();
    if (strategy == ByteScanStrategy::Avx2) return cpu.avx2;
    if (strategy == ByteScanStrategy::Avx512) return cpu.avx512f && cpu.avx512bw;
#endif
    return false;
}
std::size_t find_byte(std::string_view s, char n) noexcept {
    static const ScanFn impl = select_find(ByteScanStrategy::Auto);
    return impl(s, n);
}
std::size_t count_byte(std::string_view s, char n) noexcept {
    static const ScanFn impl = select_count(ByteScanStrategy::Auto);
    return impl(s, n);
}
std::size_t find_byte_using(std::string_view s, char n, ByteScanStrategy strategy) noexcept {
    return select_find(strategy)(s, n);
}
std::size_t count_byte_using(std::string_view s, char n, ByteScanStrategy strategy) noexcept {
    return select_count(strategy)(s, n);
}
} // namespace newton::simd
