#include <newton/simd/byte_scan.hpp>
#include <chrono>
#include <iostream>
#include <string>
int main() {
    std::string sample(1 << 20, 'a');
    for (std::size_t i = 0; i < sample.size(); i += 89) sample[i] = ',';
    volatile std::size_t sink = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) sink = sink + newton::simd::count_byte(sample, ',');
    const auto end = std::chrono::steady_clock::now();
    std::cout << "100 x 1 MiB scans in " << std::chrono::duration<double, std::milli>(end - start).count()
              << " ms; checksum=" << sink << '\n';
}
