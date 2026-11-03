#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define NEWTON_FILTER_X86_AVX2 1
#endif
#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#define NEWTON_FILTER_ARM_NEON 1
#endif

namespace newton::columnar {
// Returns indices of unsigned 64-bit values strictly greater than threshold.
// The caller supplies an output span of size >= values.size(). No allocations.
// Runtime ISA checks are cached locally, with no daemon or global context.
// Scalar path remains available on every compiler and architecture.
namespace detail {
inline std::size_t select_gt_scalar(
        std::span<const std::uint64_t> values, std::uint64_t threshold,
        std::span<std::uint32_t> result) noexcept {
    std::size_t count = 0;
    for (std::size_t i = 0; i < values.size(); ++i)
        if (values[i] > threshold) result[count++] = static_cast<std::uint32_t>(i);
    return count;
}

#ifdef NEWTON_FILTER_X86_AVX2
__attribute__((target("avx2"))) inline std::size_t select_gt_avx2(
        std::span<const std::uint64_t> values, std::uint64_t threshold,
        std::span<std::uint32_t> result) noexcept {
    const auto sign = _mm256_set1_epi64x(static_cast<long long>(0x8000000000000000ULL));
    const auto needle = _mm256_xor_si256(
        _mm256_set1_epi64x(static_cast<long long>(threshold)), sign);
    std::size_t i = 0, count = 0;
    for (; values.size() - i >= 4; i += 4) {
        const auto raw = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(values.data() + i));
        const auto cmp = _mm256_cmpgt_epi64(_mm256_xor_si256(raw, sign), needle);
        auto mask = static_cast<unsigned>(_mm256_movemask_pd(_mm256_castsi256_pd(cmp)));
        while (mask) {
            const auto bit = std::countr_zero(mask);
            result[count++] = static_cast<std::uint32_t>(i + bit);
            mask &= mask - 1;
        }
    }
    for (; i < values.size(); ++i)
        if (values[i] > threshold) result[count++] = static_cast<std::uint32_t>(i);
    return count;
}
inline bool has_avx2() noexcept {
    static const bool supported = []() noexcept {
        __builtin_cpu_init();
        return __builtin_cpu_supports("avx2") != 0;
    }();
    return supported;
}
#endif

#ifdef NEWTON_FILTER_ARM_NEON
inline std::size_t select_gt_neon(
        std::span<const std::uint64_t> values, std::uint64_t threshold,
        std::span<std::uint32_t> result) noexcept {
    const auto needle = vdupq_n_u64(threshold);
    std::size_t i = 0, count = 0;
    for (; values.size() - i >= 2; i += 2) {
        const auto mask = vcgtq_u64(vld1q_u64(values.data() + i), needle);
        if (vgetq_lane_u64(mask, 0))
            result[count++] = static_cast<std::uint32_t>(i);
        if (vgetq_lane_u64(mask, 1))
            result[count++] = static_cast<std::uint32_t>(i + 1);
    }
    for (; i < values.size(); ++i)
        if (values[i] > threshold) result[count++] = static_cast<std::uint32_t>(i);
    return count;
}
#endif
} // namespace detail

inline std::size_t select_gt_u64_into(
        std::span<const std::uint64_t> values, std::uint64_t threshold,
        std::span<std::uint32_t> output) {
    if (values.size() > std::numeric_limits<std::uint32_t>::max() ||
        output.size() < values.size())
        throw std::length_error("Selection requires row_count of 32-bit output slots");
    if (values.size() < 64)
        return detail::select_gt_scalar(values, threshold, output);
#ifdef NEWTON_FILTER_X86_AVX2
    if (detail::has_avx2())
        return detail::select_gt_avx2(values, threshold, output);
#endif
#ifdef NEWTON_FILTER_ARM_NEON
    return detail::select_gt_neon(values, threshold, output);
#else
    return detail::select_gt_scalar(values, threshold, output);
#endif
}
} // namespace newton::columnar
