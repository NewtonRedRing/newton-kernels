#include <newton/analytics/dense_join.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <vector>

struct Aggregate { std::uint64_t sum = 0; std::size_t count = 0; };
std::uint64_t mix(std::uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
static Aggregate std_join(const std::vector<std::uint64_t>& left,
                          const std::vector<std::uint64_t>& right,
                          std::size_t unique) {
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> index;
    index.reserve(std::min(right.size(), unique));
    for (std::size_t i = 0; i < right.size(); ++i) index[right[i]].push_back(i);
    Aggregate a;
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto it = index.find(left[i]);
        if (it != index.end()) for (auto j : it->second) {
            a.sum += mix((static_cast<std::uint64_t>(i) << 32) | j); ++a.count;
        }
    }
    return a;
}
static Aggregate dense_join(const std::vector<std::uint64_t>& left,
                            const std::vector<std::uint64_t>& right,
                            std::vector<std::size_t>& heads,
                            std::vector<std::size_t>& next) {
    Aggregate a;
    a.count = newton::analytics::dense_join_into(left, right, 0, heads, next,
        [&](std::size_t i, std::size_t j) {
            a.sum += mix((static_cast<std::uint64_t>(i) << 32) | j);
        });
    return a;
}
static double median(std::vector<double> v) {
    std::sort(v.begin(), v.end()); return (v[v.size()/2] + v[(v.size()-1)/2]) / 2;
}
int main() {
    volatile std::uint64_t sink = 0;
    for (std::size_t width : {256u, 4096u, 65536u, 262144u}) {
        std::mt19937_64 rng(0x43521678 + width);
        std::vector<std::uint64_t> left(32000), right(16000);
        for (auto& k : left) k = rng() % width;
        for (auto& k : right) k = rng() % width;
        std::vector<std::size_t> heads(width), next(right.size());
        if (dense_join(left,right,heads,next).count != std_join(left,right,width).count ||
            dense_join(left,right,heads,next).sum != std_join(left,right,width).sum)
            throw std::runtime_error("Output mismatch");
        std::vector<double> dense_ms, std_ms;
        for (int round=0; round<20; ++round) {
            const bool standard_first = (round % 2) == 0;
            const auto bench = [&](bool standard) {
                const auto start = std::chrono::steady_clock::now();
                const auto result = standard ? std_join(left,right,width) : dense_join(left,right,heads,next);
                const auto end = std::chrono::steady_clock::now();
                sink = sink ^ result.sum ^ static_cast<std::uint64_t>(result.count);
                const double elapsed=std::chrono::duration<double,std::milli>(end-start).count();
                (standard?std_ms:dense_ms).push_back(elapsed);
            };
            bench(standard_first); bench(!standard_first);
        }
        const auto d=median(dense_ms), s=median(std_ms);
        std::cout << "domain=" << width << " left=32000 right=16000"
                  << " pairs=" << dense_join(left,right,heads,next).count
                  << " dense_median_ms=" << std::fixed << std::setprecision(5) << d
                  << " stl_median_ms=" << s << " stl/dense=" << s/d << "x\n";
    }
    std::cerr << "checksum_sanity=" << sink << '\n';
}
