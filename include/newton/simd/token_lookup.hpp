#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define NEWTON_TOKEN_X86 1
#endif

namespace newton::simd {
namespace detail {
inline std::uint32_t token_mask_scalar(std::string_view data) noexcept {
    std::uint32_t bits = 0;
    const auto n = data.size() < 32 ? data.size() : 32;
    for (std::size_t i = 0; i < n; ++i) {
        const auto c = static_cast<unsigned char>(data[i]);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9')) bits |= std::uint32_t{1} << i;
    }
    return bits;
}
#ifdef NEWTON_TOKEN_X86
__attribute__((target("avx2"))) inline std::uint32_t token_mask_avx2(const char* data) noexcept {
    const auto v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data));
    const auto al = _mm256_and_si256(
        _mm256_cmpgt_epi8(v, _mm256_set1_epi8('a' - 1)),
        _mm256_cmpgt_epi8(_mm256_set1_epi8('z' + 1), v));
    const auto au = _mm256_and_si256(
        _mm256_cmpgt_epi8(v, _mm256_set1_epi8('A' - 1)),
        _mm256_cmpgt_epi8(_mm256_set1_epi8('Z' + 1), v));
    const auto di = _mm256_and_si256(
        _mm256_cmpgt_epi8(v, _mm256_set1_epi8('0' - 1)),
        _mm256_cmpgt_epi8(_mm256_set1_epi8('9' + 1), v));
    return static_cast<std::uint32_t>(_mm256_movemask_epi8(
        _mm256_or_si256(_mm256_or_si256(al, au), di)));
}
__attribute__((target("avx2"))) inline std::array<std::uint64_t, 4> gather_avx2(
        const std::uint64_t* table, const std::array<std::size_t, 4>& loc) noexcept {
    const auto indexes = _mm256_set_epi64x(
        static_cast<long long>(loc[3]), static_cast<long long>(loc[2]),
        static_cast<long long>(loc[1]), static_cast<long long>(loc[0]));
    const auto v = _mm256_i64gather_epi64(
        reinterpret_cast<const long long*>(table), indexes, 8);
    std::array<std::uint64_t, 4> result{};
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(result.data()), v);
    return result;
}
inline bool token_has_avx2() noexcept {
    static const bool available = []() noexcept {
        __builtin_cpu_init();
        return __builtin_cpu_supports("avx2") != 0;
    }();
    return available;
}
#endif
} // namespace detail

// ASCII alphanumeric classification for the first 0..32 bytes.
// A bit is set for each [A-Z], [a-z], or [0-9] byte. Reject non-ASCII.
inline std::uint32_t ascii_token_mask32(std::string_view data) noexcept {
#ifdef NEWTON_TOKEN_X86
    if (data.size() >= 32 && detail::token_has_avx2())
        return detail::token_mask_avx2(data.data());
#endif
    return detail::token_mask_scalar(data);
}

// Four independent bounded table loads. Collision resolution requires
// comparison against the FULL original key at the caller level.
inline std::array<std::uint64_t, 4> gather4_u64(
    std::span<const std::uint64_t> table,
    const std::array<std::size_t, 4>& indices) {
    for (auto index : indices)
        if (index >= table.size()) throw std::out_of_range("SIMD gather index");
#ifdef NEWTON_TOKEN_X86
    if (detail::token_has_avx2()) return detail::gather_avx2(table.data(), indices);
#endif
    return {table[indices[0]], table[indices[1]], table[indices[2]], table[indices[3]]};
}
} // namespace newton::simd
