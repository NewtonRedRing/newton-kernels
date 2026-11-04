#include <newton/core/cpu_features.hpp>

namespace newton::core {

// CPU feature detection is process-wide for the supported x86 toolchains.
// Cache the process-usable snapshot to avoid repeating __builtin_cpu_init()
// and feature lookups in hot-path dispatch code. Initialization is thread-safe
// since C++11; the return type and nonthrowing public API are unchanged.
CpuFeatures cpu_features() noexcept {
    static const CpuFeatures snapshot = []() noexcept {
        CpuFeatures features{};
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
        __builtin_cpu_init();
        features.sse42 = __builtin_cpu_supports("sse4.2");
        features.avx2 = __builtin_cpu_supports("avx2");
        features.avx512f = __builtin_cpu_supports("avx512f");
        features.avx512bw = __builtin_cpu_supports("avx512bw");
#endif
        return features;
    }();
    return snapshot;
}
} // namespace newton::core
