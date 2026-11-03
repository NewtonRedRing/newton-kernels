#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define NEWTON_CSV_MASK_X86 1
#endif

namespace newton::parsing {
// Raw CSV structural candidates, not validation: commas/newlines OUTSIDE a
// quote span. Pair-escaped double quotes do not change subsequent parity.
// Quote placement validation remains the job of the strict CSV parser.
// The caller carries inside_quotes between consecutive blocks.
struct CsvStructureMask {
    std::uint64_t commas = 0;
    std::uint64_t line_ends = 0; // CR and LF independently, including CRLF.
    bool inside_quotes = false;
};
namespace detail {
inline CsvStructureMask csv_mask_scalar(
    const char* input, std::size_t n, bool inside) noexcept {
    CsvStructureMask out;
    for (std::size_t i = 0; i < n; ++i) {
        const auto ch = input[i];
        if (ch == '"') inside = !inside;
        else if (!inside) {
            if (ch == ',') out.commas |= std::uint64_t{1} << i;
            if (ch == '\n' || ch == '\r') out.line_ends |= std::uint64_t{1} << i;
        }
    }
    out.inside_quotes = inside;
    return out;
}
#ifdef NEWTON_CSV_MASK_X86
__attribute__((target("avx2,pclmul"))) inline CsvStructureMask csv_mask_accel(
    const char* input, std::size_t n, bool inside) noexcept {
    alignas(32) char tail[64]{};
    if (n < 64) {
        std::memcpy(tail, input, n);
        input = tail;
    }
    const auto first = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input));
    const auto second = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + 32));
    auto eq64 = [&](char ch) noexcept -> std::uint64_t {
        const auto symbol = _mm256_set1_epi8(ch);
        const auto lo = static_cast<std::uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(first, symbol)));
        const auto hi = static_cast<std::uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(second, symbol)));
        return std::uint64_t{lo} | (std::uint64_t{hi} << 32);
    };
    const std::uint64_t quote = eq64('"');
    const auto bits = _mm_cvtsi64_si128(static_cast<long long>(quote));
    const auto ones = _mm_cvtsi64_si128(-1LL);
    const auto parity = _mm_clmulepi64_si128(bits, ones, 0x00);
    std::uint64_t inside_mask = static_cast<std::uint64_t>(_mm_cvtsi128_si64(parity));
    if (inside) inside_mask = ~inside_mask;
    const auto valid = n == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << n) - 1;
    CsvStructureMask out;
    const auto structural = (~inside_mask) & valid;
    out.commas = eq64(',') & structural;
    out.line_ends = (eq64('\n') | eq64('\r')) & structural;
    out.inside_quotes = inside != ((std::popcount(quote & valid) & 1) != 0);
    return out;
}
inline bool have_avx2_pclmul() noexcept {
    static const bool supported = []() noexcept {
        __builtin_cpu_init();
        return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("pclmul");
    }();
    return supported;
}
#endif
} // namespace detail

inline CsvStructureMask csv_structure_mask64(
    const char* bytes, std::size_t size, bool inside_quotes = false) {
    if (size > 64 || (bytes == nullptr && size != 0))
        throw std::invalid_argument("CSV structural block must contain 0 to 64 bytes");
    if (size == 0) return {0, 0, inside_quotes};
#ifdef NEWTON_CSV_MASK_X86
    if (size >= 32 && detail::have_avx2_pclmul())
        return detail::csv_mask_accel(bytes, size, inside_quotes);
#endif
    return detail::csv_mask_scalar(bytes, size, inside_quotes);
}
} // namespace newton::parsing
