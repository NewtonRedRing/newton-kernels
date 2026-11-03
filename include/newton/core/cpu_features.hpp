#pragma once
namespace newton::core {
struct CpuFeatures {
    bool sse42 = false;
    bool avx2 = false;
    bool avx512f = false;
    bool avx512bw = false;
};
// Reports process-usable instruction support (not merely hardware flags).
// Lazily snapshotted at first call; subsequent calls do not repeat detection.
[[nodiscard]] CpuFeatures cpu_features() noexcept;
} // namespace newton::core
