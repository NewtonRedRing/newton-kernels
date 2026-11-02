#include <newton/analytics/percentile.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
std::uint64_t rng = 0x72e4db519caa7ULL;
std::uint64_t next_random() {
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
    return rng;
}
template <typename F> double median_ms(F&& fn) {
    std::vector<double> samples;
    fn();
    for (int i = 0; i < 9; ++i) {
        const auto a = std::chrono::steady_clock::now();
        fn();
        samples.push_back(std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - a).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[4];
}
}
int main() {
    constexpr int iterations = 40;
    std::vector<std::uint64_t> data(100000);
    for (auto& v : data) v = next_random() % 10000000;
    using newton::analytics::PercentileStrategy;
    using newton::analytics::percentile_nearest_rank_using;
    std::uint64_t sink = 0;
    const double selection = median_ms([&] {
        std::uint64_t result = 0;
        for (int i = 0; i < iterations; ++i)
            result += percentile_nearest_rank_using(data, 0.95, PercentileStrategy::Selection);
        sink = result;
    });
    const auto expected = sink;
    const double sorting = median_ms([&] {
        std::uint64_t result = 0;
        for (int i = 0; i < iterations; ++i)
            result += percentile_nearest_rank_using(data, 0.95, PercentileStrategy::FullSort);
        sink = result;
    });
    if (expected != sink) return 2;
    std::cout << "Percentile (100000 values, p95, 40 queries; median of 9)\n"
              << "  selection_ms=" << selection << "\n"
              << "  full_sort_ms=" << sorting << "\n"
              << "checksum=" << sink << '\n';
}
