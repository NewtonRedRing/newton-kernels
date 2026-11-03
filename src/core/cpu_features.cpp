#include <newton/core/cpu_features.hpp>
namespace newton::core {
CpuFeatures cpu_features() noexcept {
    CpuFeatures features{};
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
    __builtin_cpu_init();
    features.sse42 = __builtin_cpu_supports("sse4.2");
    features.avx2 = __builtin_cpu_supports("avx2");
    features.avx512f = __builtin_cpu_supports("avx512f");
    features.avx512bw = __builtin_cpu_supports("avx512bw");
#endif
    return features;
}
// Explicitly cached variant; the established runtime query remains unchanged.
CpuFeatures cpu_features_cached() noexcept {
    static const CpuFeatures snapshot = cpu_features();
    return snapshot;
}
} // namespace newton::core
