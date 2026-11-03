#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace newton::core {
// Explicit, caller-owned phase timings; no heap allocation, global registry,
// hidden worker or background sampling. Keep outside production hot paths.
struct PhaseSample {
    std::string_view name;
    std::uint64_t elapsed_ns = 0;
};
class ScopedPhase {
public:
    explicit ScopedPhase(PhaseSample& sample) noexcept
        : sample_(&sample), begin_(Clock::now()) {}
    ~ScopedPhase() noexcept {
        if (sample_ != nullptr) {
            const auto end = Clock::now();
            sample_->elapsed_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - begin_).count());
        }
    }
    ScopedPhase(const ScopedPhase&) = delete;
    ScopedPhase& operator=(const ScopedPhase&) = delete;
private:
    using Clock = std::chrono::steady_clock;
    PhaseSample* sample_;
    Clock::time_point begin_;
};
} // namespace newton::core
