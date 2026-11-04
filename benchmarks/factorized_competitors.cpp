// Fairer aggregate-only competitive screen: each strategy includes index
// construction and grouped aggregation. All three compute identical results.
// No output-pair enumeration in any variant. No external runtime dependencies.
#include <newton/analytics/factorized_dense_join.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace {
constexpr std::size_t GROUPS = 16;
struct Result {
    std::size_t pairs = 0;
    std::array<std::uint64_t, GROUPS> counts{}, sums{};
    bool operator==(const Result&) const = default;
};
struct Fixture {
    std::vector<std::uint64_t> left, right, values;
    std::vector<std::uint32_t> groups;
    std::size_t width;
};
std::size_t allocation_calls = 0, allocation_bytes = 0;
template<class T> struct TrackingAllocator {
    using value_type = T;
    TrackingAllocator() noexcept = default;
    template<class U> TrackingAllocator(const TrackingAllocator<U>&) noexcept {}
    T* allocate(std::size_t n) {
        ++allocation_calls;
        allocation_bytes += n * sizeof(T);
        return std::allocator<T>{}.allocate(n);
    }
    void deallocate(T* ptr, std::size_t n) noexcept {
        std::allocator<T>{}.deallocate(ptr,n);
    }
    template<class U> bool operator==(const TrackingAllocator<U>&) const noexcept {return true;}
};

Fixture make_fixture(std::size_t width, std::uint64_t seed) {
    Fixture f;
    f.width = width;
    f.left.resize(32000);
    f.right.resize(16000);
    f.groups.resize(f.left.size());
    f.values.resize(f.left.size());
    std::mt19937_64 rng(seed);
    for (auto& x : f.left) x = rng()%width;
    for (auto& x : f.right) x = rng()%width;
    for (auto& x : f.values) x = rng()%200;
    for (auto& x : f.groups) x = static_cast<std::uint32_t>(rng()%GROUPS);
    return f;
}
void add(Result& out, std::size_t i, std::size_t n, const Fixture& f) {
    if (!n) return;
    const auto group = f.groups[i];
    const auto val = f.values[i];
    out.pairs += n;
    out.counts[group] += n;
    out.sums[group] += val*n;
}
Result dense(const Fixture& f, std::size_t& scratch_bytes) {
    std::vector<std::size_t> frequencies(f.width);
    scratch_bytes = frequencies.capacity()*sizeof(std::size_t);
    Result out;
    out.pairs = newton::analytics::factorized_dense_join_group_sum_into(
        f.left, f.groups, f.values, f.right, 0, frequencies,
        out.counts, out.sums);
    return out;
}
Result hash(const Fixture& f, std::size_t& scratch_bytes) {
    using Pair = std::pair<const std::uint64_t, std::size_t>;
    using Map = std::unordered_map<std::uint64_t, std::size_t,
                  std::hash<std::uint64_t>, std::equal_to<std::uint64_t>,
                  TrackingAllocator<Pair>>;
    Map counts;
    // A strong baseline: explicitly reserve expected distinct keys.
    counts.reserve(std::min(f.width, f.right.size()));
    for (const auto key : f.right) ++counts[key];
    scratch_bytes = counts.bucket_count()*sizeof(void*) +
        counts.size()*(sizeof(Pair)+sizeof(void*)); // estimate only
    Result out;
    for (std::size_t i=0; i<f.left.size(); ++i) {
        const auto found = counts.find(f.left[i]);
        if (found != counts.end()) add(out, i, found->second, f);
    }
    return out;
}
Result sorted(const Fixture& f, std::size_t& scratch_bytes) {
    auto keys = f.right;
    std::sort(keys.begin(),keys.end());
    scratch_bytes = keys.capacity()*sizeof(std::uint64_t);
    Result out;
    for (std::size_t i=0; i<f.left.size(); ++i) {
        const auto range = std::equal_range(keys.begin(),keys.end(),f.left[i]);
        add(out, i, static_cast<std::size_t>(range.second-range.first), f);
    }
    return out;
}
double percentile(std::vector<double> values, double pct) {
    std::sort(values.begin(), values.end());
    const double index = pct*static_cast<double>(values.size()-1);
    const auto low = static_cast<std::size_t>(index);
    const auto high = std::min(low+1,values.size()-1);
    return values[low]+(values[high]-values[low])*(index-low);
}
struct Measurement {
    std::vector<double> ms;
    std::size_t allocated_bytes=0, allocation_count=0, scratch_bytes=0;
};
std::uint64_t digest(const Result& result) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    const auto mix=[&](std::uint64_t x) mutable {hash^=x;hash*=0x100000001b3ULL;};
    mix(result.pairs);
    for (const auto x : result.counts) mix(x);
    for (const auto x : result.sums) mix(x);
    return hash;
}
std::size_t rss_kib() {
#if defined(__unix__) || defined(__APPLE__)
    struct rusage ru{};
    if (getrusage(RUSAGE_SELF,&ru)==0) {
    #ifdef __APPLE__
        return static_cast<std::size_t>(ru.ru_maxrss/1024);
    #else
        return static_cast<std::size_t>(ru.ru_maxrss);
    #endif
    }
#endif
    return 0;
}
} // namespace
int main(int argc, char** argv) {
    try {
        int repetitions=45;
        if (argc==2) repetitions=std::stoi(argv[1]);
        if (repetitions<9 || repetitions>1000)
            throw std::invalid_argument("repetitions must be 9..1000");
        std::cout << "benchmark,domain,variant,pairs,p50_ms,p95_ms,p99_ms,"
                     "throughput_mrows_s,tracked_alloc_calls,tracked_alloc_bytes,"
                     "estimated_scratch_bytes,process_peak_rss_kib,checksum\n";
        for (const std::size_t width : {64u,256u,4096u,65536u,262144u,1048576u}) {
            const Fixture f = make_fixture(width, 0xb4511735u + width);
            std::array<Measurement,3> metrics;
            auto run = [&](int variant, bool measured) {
                std::size_t scratch=0;
                allocation_calls = allocation_bytes = 0;
                const auto start=std::chrono::steady_clock::now();
                Result r = variant==0 ? dense(f,scratch) :
                           variant==1 ? hash(f,scratch) : sorted(f,scratch);
                const auto end=std::chrono::steady_clock::now();
                if (measured) {
                    auto& m=metrics[variant];
                    m.ms.push_back(std::chrono::duration<double,std::milli>(end-start).count());
                    m.scratch_bytes = scratch;
                    m.allocated_bytes += allocation_bytes;
                    m.allocation_count += allocation_calls;
                }
                return r;
            };
            const Result reference=run(1,false);
            for (int i=0;i<3;++i)
                if (!(run(i,false)==reference))
                    throw std::runtime_error("Competitor result mismatch before measurement");
            // Three rotations, then reverse rotations; less order bias than
            // measuring a full set of one strategy followed by another.
            for (int i=0;i<repetitions;++i) {
                const std::array<int,3> order = i%2 ?
                    std::array<int,3>{2-(i%3),1-(i%3)+((i%3)==2?3:0),0} :
                    std::array<int,3>{i%3,(i+1)%3,(i+2)%3};
                // Explicitly normalize the reverse cycle for all residues.
                for (int k=0;k<3;++k) {
                    const int variant = i%2 ? (i%3+3-k)%3 : order[k];
                    if (!(run(variant,true)==reference))
                        throw std::runtime_error("Competitor result drift during measurement");
                }
            }
            for (int variant=0;variant<3;++variant) {
                const auto& m=metrics[variant];
                const auto p50=percentile(m.ms,0.5);
                std::cout << "aggregate_only," << width << ","
                          << (variant==0?"dense_histogram":variant==1?"reserved_std_hash":"sorted_equal_range")
                          << "," << reference.pairs << "," << std::setprecision(8)
                          << p50 << "," << percentile(m.ms,0.95) << ","
                          << percentile(m.ms,0.99) << ","
                          << (f.left.size()+f.right.size())/(1000.0*p50) << ","
                          << m.allocation_count / static_cast<std::size_t>(repetitions) << ","
                          << m.allocated_bytes / static_cast<std::size_t>(repetitions) << ","
                          << m.scratch_bytes << "," << rss_kib() << ","
                          << digest(reference) << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Factorized competitor benchmark failed: " << e.what() << "\n";
        return 2;
    }
}
