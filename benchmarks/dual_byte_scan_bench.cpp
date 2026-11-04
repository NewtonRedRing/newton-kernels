#include <newton/simd/byte_scan.hpp>
#include <newton/simd/count_two.hpp>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

static double median(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const auto n = samples.size();
    return (samples[(n-1)/2] + samples[n/2]) * 0.5;
}
int main() {
    const char a = ',', b = '\n';
    std::mt19937_64 rng(0x421b31843);
    for (const std::size_t n : {std::size_t{1} << 20, std::size_t{16} << 20}) {
        std::string bytes(n, 'x');
        for (auto& ch : bytes) {
            const auto r = rng() & 255;
            ch = r < 16 ? a : r < 32 ? b : static_cast<char>(r);
        }
        const std::string_view view = bytes;
        const auto expected = static_cast<std::size_t>(std::count_if(
            view.begin(), view.end(), [a,b](char ch) {return ch == a || ch == b;}));
        if (expected != newton::simd::count_either_byte(view,a,b) ||
            expected != newton::simd::count_byte(view,a) + newton::simd::count_byte(view,b))
            throw std::runtime_error("SIMD/standard delimiter counts disagree");
        std::vector<double> std_times, two_pass_times, fused_times;
        volatile std::size_t sink = 0;
        const auto measure = [&](int variant, std::vector<double>& out) {
            const auto start = std::chrono::steady_clock::now();
            std::size_t result;
            if (variant == 0) result = static_cast<std::size_t>(std::count_if(
                view.begin(), view.end(), [a,b](char ch) {return ch == a || ch == b;}));
            else if (variant == 1) result =
                newton::simd::count_byte(view,a) + newton::simd::count_byte(view,b);
            else result = newton::simd::count_either_byte(view,a,b);
            const auto end = std::chrono::steady_clock::now();
            if (result != expected) throw std::runtime_error("Incorrect count");
            sink = sink ^ result;
            out.push_back(std::chrono::duration<double, std::milli>(end-start).count());
        };
        // Interleave multiple operation orders to reduce clock/thermal bias.
        for (int round=0; round<15; ++round) {
            if (round%3==0) {
                measure(0,std_times); measure(1,two_pass_times); measure(2,fused_times);
            } else if (round%3==1) {
                measure(1,two_pass_times); measure(2,fused_times); measure(0,std_times);
            } else {
                measure(2,fused_times); measure(0,std_times); measure(1,two_pass_times);
            }
        }
        std::cout << std::fixed << std::setprecision(6)
                  << "size_bytes=" << n << " matches=" << expected
                  << " fused_ms=" << median(fused_times)
                  << " std_count_if_ms=" << median(std_times)
                  << " two_count_byte_ms=" << median(two_pass_times)
                  << " std/fused=" << median(std_times)/median(fused_times)
                  << " two_pass/fused=" << median(two_pass_times)/median(fused_times)
                  << " checksum=" << sink << '\n';
    }
}
