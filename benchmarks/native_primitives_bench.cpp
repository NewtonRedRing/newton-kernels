// Independent screening benchmark: no contestant code or claimed universal wins.
// Compare exact results on identical generated inputs and publish losses.
#include <newton/analytics/dense_group.hpp>
#include <newton/analytics/percentile.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
volatile std::uint64_t observed = 0;
template<class Fn>
double median_ms(Fn&& fn) {
    std::array<double, 9> timings{};
    for (auto& ms : timings) {
        const auto begin = Clock::now();
        observed = fn();
        const auto end = Clock::now();
        ms = std::chrono::duration<double, std::milli>(end - begin).count();
    }
    std::sort(timings.begin(), timings.end());
    return timings[timings.size() / 2];
}
std::uint64_t digest(std::span<const std::size_t> counts,
                     std::span<const std::uint64_t> sums) {
    std::uint64_t h = 14695981039346656037ULL;
    for (std::size_t i = 0; i < counts.size(); ++i) {
        h = (h ^ static_cast<std::uint64_t>(counts[i])) * 1099511628211ULL;
        h = (h ^ sums[i]) * 1099511628211ULL;
    }
    return h;
}
void group() {
    constexpr std::size_t n = 1000000, domain = 256;
    std::mt19937_64 rng{20261103};
    std::vector<std::uint32_t> keys(n);
    std::vector<std::uint64_t> values(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys[i] = static_cast<std::uint32_t>(rng() % domain);
        values[i] = rng() % 1000;
    }
    std::vector<std::size_t> counts(domain);
    std::vector<std::uint64_t> sums(domain);
    const auto fast = [&] {
        newton::analytics::dense_group_count_sum_into(keys, values, counts, sums);
        return digest(counts, sums);
    };
    const auto direct = [&] {
        std::fill(counts.begin(), counts.end(), 0);
        std::fill(sums.begin(), sums.end(), 0);
        for (std::size_t i = 0; i < n; ++i) {
            ++counts[keys[i]];
            sums[keys[i]] += values[i];
        }
        return digest(counts, sums);
    };
    const auto hash = [&] {
        std::unordered_map<std::uint32_t, std::pair<std::size_t, std::uint64_t>> map;
        map.reserve(domain);
        for (std::size_t i = 0; i < n; ++i) {
            auto& result = map[keys[i]];
            ++result.first;
            result.second += values[i];
        }
        std::fill(counts.begin(), counts.end(), 0);
        std::fill(sums.begin(), sums.end(), 0);
        for (const auto& [key, pair] : map) {
            counts[key] = pair.first;
            sums[key] = pair.second;
        }
        return digest(counts, sums);
    };
    if (fast() != direct() || fast() != hash())
        throw std::runtime_error("Dense group output mismatch");
    const auto a = median_ms(fast);
    const auto b = median_ms(direct);
    const auto c = median_ms(hash);
    std::cout << "dense_count_sum: rows=1000000 keys=256\n"
              << "  newton_checked=" << a << "ms, direct_unchecked=" << b
              << "ms, std_unordered_map=" << c << "ms\n";
}
void percentile() {
    constexpr std::size_t n = 500000, domain = 1024;
    std::mt19937_64 rng{20261104};
    std::vector<std::uint64_t> values(n);
    for (auto& v : values) v = rng() % domain;
    const std::array<double, 3> qs{0.5, 0.95, 0.99};
    std::array<std::uint64_t, 3> output{};
    std::vector<std::size_t> counts(domain);
    const auto checksum = [&] {
        return output[0] ^ (output[1] << 16) ^ (output[2] << 32);
    };
    const auto histogram = [&] {
        newton::analytics::percentiles_nearest_rank_bounded_into(
            values, 0, counts, qs, output);
        return checksum();
    };
    const auto selection = [&] {
        for (std::size_t i = 0; i < qs.size(); ++i)
            output[i] = newton::analytics::percentile_nearest_rank(values, qs[i]);
        return checksum();
    };
    const auto sort_once = [&] {
        auto copy = values;
        std::sort(copy.begin(), copy.end());
        for (std::size_t i = 0; i < qs.size(); ++i) {
            const auto rank = static_cast<std::size_t>(std::ceil(qs[i] * static_cast<double>(n)));
            output[i] = copy[rank == 0 ? 0 : rank - 1];
        }
        return checksum();
    };
    if (histogram() != selection() || histogram() != sort_once())
        throw std::runtime_error("Bounded exact percentile output mismatch");
    const auto a = median_ms(histogram);
    const auto b = median_ms(selection);
    const auto c = median_ms(sort_once);
    std::cout << "percentiles: rows=500000 domain=1024 p50/p95/p99\n"
              << "  newton_histogram=" << a << "ms, three_selections=" << b
              << "ms, one_sort=" << c << "ms\n";
}
} // namespace
int main() {
    try {
        std::cout << std::fixed << std::setprecision(4);
        group();
        percentile();
        std::cout << "SCREENING ONLY: local warm-cache timings, no controlled CPU frequency.\n";
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
