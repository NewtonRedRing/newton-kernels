#include <newton/simd/byte_scan.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {
template<class F> double median_ms(F&& f) {
    std::vector<double> samples;
    f();
    for (int i = 0; i < 9; ++i) {
        const auto start = std::chrono::steady_clock::now();
        f();
        samples.push_back(std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[4];
}
} // namespace
int main() {
    using newton::simd::ByteScanStrategy;
    constexpr std::size_t bytes = 1 << 20;
    constexpr int iterations = 256;
    std::string data(bytes, 'a');
    for (std::size_t i = 17; i < bytes; i += 89) data[i] = ',';
    const std::size_t reference = newton::simd::count_byte_using(data, ',', ByteScanStrategy::Scalar);
    std::uint64_t sink = 0;
    const std::array variants{
        std::pair{ByteScanStrategy::Scalar, "scalar"},
        std::pair{ByteScanStrategy::Avx2, "avx2"},
        std::pair{ByteScanStrategy::Avx512, "avx512"},
        std::pair{ByteScanStrategy::Auto, "auto"}};
    std::cout << "Scan: " << bytes << " bytes x " << iterations << " iterations, median of 9 runs; "
              << "expected_count=" << reference << '\n';
    for (const auto [variant, label] : variants) {
        if (!newton::simd::supports_byte_scan(variant)) {
            std::cout << label << "=unsupported\n";
            continue;
        }
        const double ms = median_ms([&] {
            std::uint64_t total = 0;
            for (int j = 0; j < iterations; ++j)
                total += newton::simd::count_byte_using(data, ',', variant);
            sink = total;
        });
        if (sink != iterations * reference) return 2;
        std::cout << std::fixed << std::setprecision(3) << label << "_ms=" << ms
                  << " gib_per_second=" << (bytes * double(iterations) / (1024. * 1024. * 1024.)) / (ms / 1000.) << '\n';
    }
    std::cout << "checksum=" << sink << '\n';
}
