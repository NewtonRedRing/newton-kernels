#include <newton/analytics/factorized_dense_join.hpp>
#include <newton/analytics/fused_join.hpp>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

struct Result {
    std::size_t pairs = 0;
    std::vector<std::uint64_t> counts, sums;
};
static double median(std::vector<double> data) {
    std::sort(data.begin(), data.end());
    return data[data.size()/2];
}
int main() {
    volatile std::uint64_t sink = 0;
    for (const std::size_t width : {64u, 256u, 4096u}) {
        std::mt19937_64 rng(0x879adc15 + width);
        std::vector<std::uint64_t> left(32000), right(16000), values(left.size());
        std::vector<std::uint32_t> groups(left.size());
        for (auto& k : left) k = rng()%width;
        for (auto& k : right) k = rng()%width;
        for (auto& v : values) v = rng()%200;
        for (auto& g : groups) g = static_cast<std::uint32_t>(rng()%16);

        // All scratch preallocated outside timing for both variants.
        std::vector<std::size_t> frequencies(width), heads(width*2), next(right.size());
        const auto factorized = [&] {
            Result out{0, std::vector<std::uint64_t>(16), std::vector<std::uint64_t>(16)};
            out.pairs = newton::analytics::factorized_dense_join_group_sum_into(
                left,groups,values,right,0,frequencies,out.counts,out.sums);
            return out;
        };
        const auto original = [&] {
            Result out{0, std::vector<std::uint64_t>(16), std::vector<std::uint64_t>(16)};
            out.pairs = newton::analytics::fused_join_group_sum_into(
                left,groups,values,right,heads,next,out.counts,out.sums);
            return out;
        };
        const auto reference = original(), actual = factorized();
        if (reference.pairs != actual.pairs ||
            reference.counts != actual.counts ||
            reference.sums != actual.sums)
            throw std::runtime_error("Factorized aggregate differs from pair enumeration");

        std::vector<double> original_ms, factorized_ms;
        const auto measure = [&](bool use_factorization) {
            const auto start = std::chrono::steady_clock::now();
            const auto result = use_factorization ? factorized() : original();
            const auto end = std::chrono::steady_clock::now();
            if (result.pairs != reference.pairs ||
                result.counts != reference.counts ||
                result.sums != reference.sums)
                throw std::runtime_error("Factorized output drifted");
            sink = sink ^ result.pairs ^ result.sums[0];
            (use_factorization ? factorized_ms : original_ms).push_back(
                std::chrono::duration<double,std::milli>(end-start).count());
        };
        for (int i=0;i<15;++i) {
            if (i%2) { measure(true); measure(false); }
            else { measure(false); measure(true); }
        }
        std::cout << std::fixed << std::setprecision(6)
                  << "left=32000 right=16000 domain=" << width
                  << " pairs=" << reference.pairs
                  << " original_ms=" << median(original_ms)
                  << " factorized_ms=" << median(factorized_ms)
                  << " original/factorized=" << median(original_ms)/median(factorized_ms)
                  << " frequency_scratch_bytes=" << frequencies.size()*sizeof(std::size_t)
                  << " original_hash_scratch_bytes=" << (heads.size()+next.size())*sizeof(std::size_t)
                  << " checksum=" << sink << '\n';
    }
}
