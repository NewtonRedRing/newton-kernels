#pragma once
namespace newton::core {
struct CpuFeatures {
    bool sse42 = false;
    bool avx2 = false;
    bool avx512f = false;
    bool avx512bw = false;
};
// Reports support available to the current process, not just CPU hardware.
[[nodiscard]] CpuFeatures cpu_features() noexcept;
// Opt-in process-wide cached snapshot for callers that can use stable CPU features.
[[nodiscard]] CpuFeatures cpu_features_cached() noexcept;
} // namespace newton::core
