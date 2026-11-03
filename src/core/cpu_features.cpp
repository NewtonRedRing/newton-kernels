#include <newton/core/cpu_features.hpp>
namespace newton::core {
CpuFeatures cpu_features() noexcept {
    // CPUID feature availability is process-wide for the supported toolchains.
    // Initialize once and return a compact snapshot instead of repeatedly
    // entering __builtin_cpu_init() in latency-sensitive caller loops.
    static const CpuFeatures features = []() noexcept {
        CpuFeatures detected{};
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
        __builtin_cpu_init();
        detected.sse42 = __builtin_cpu_supports("sse4.2");
        detected.avx2 = __builtin_cpu_supports("avx2");
        detected.avx512f = __builtin_cpu_supports("avx512f");
        detected.avx512bw = __builtin_cpu_supports("avx512bw");
#endif
        return detected;
    }();
    return features;
}
} // namespace newton::core
