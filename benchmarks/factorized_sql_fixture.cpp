// Same-fixture aggregate-only competitor endpoint for DuckDB SQL screening.
// Time ONLY right-side histogram build and grouped aggregate; data generation,
// validation and allocations for reusable caller-owned scratch are excluded.
#include <newton/analytics/factorized_dense_join.hpp>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif
namespace {
constexpr std::size_t GROUPS=16;
std::size_t rss_kib() {
#if defined(__unix__) || defined(__APPLE__)
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF,&usage)==0) {
#ifdef __APPLE__
        return static_cast<std::size_t>(usage.ru_maxrss/1024);
#else
        return static_cast<std::size_t>(usage.ru_maxrss);
#endif
    }
#endif
    return 0;
}
template<class T> void emit_array(const std::array<T,GROUPS>& a) {
    std::cout << "[";
    for(std::size_t i=0;i<a.size();++i) {
        if(i) std::cout << ",";
        std::cout << a[i];
    }
    std::cout << "]";
}
}
int main(int argc,char** argv) {
    try {
        if(argc != 4) throw std::invalid_argument("usage: binary WIDTH LEFT_ROWS RIGHT_ROWS");
        const auto width=static_cast<std::size_t>(std::stoull(argv[1]));
        const auto left_n=static_cast<std::size_t>(std::stoull(argv[2]));
        const auto right_n=static_cast<std::size_t>(std::stoull(argv[3]));
        if(!width || width>1000000 || left_n>1000000 || right_n>1000000)
            throw std::invalid_argument("fixture limit exceeded");
        std::vector<std::uint64_t> left(left_n), values(left_n), right(right_n);
        std::vector<std::uint32_t> groups(left_n);
        for(std::size_t i=0;i<left_n;++i) {
            left[i]=(i*17+23)%width;
            groups[i]=static_cast<std::uint32_t>((i*13+3)%GROUPS);
            values[i]=(i*29+5)%200;
        }
        for(std::size_t i=0;i<right_n;++i) right[i]=(i*31+7)%width;
        std::vector<std::size_t> frequency(width);
        std::array<std::uint64_t,GROUPS> counts{}, sums{};
        // A warm-up with matching output; avoids timing first-touch artifacts.
        (void)newton::analytics::factorized_dense_join_group_sum_into(
            left,groups,values,right,0,frequency,counts,sums);
        const auto start=std::chrono::steady_clock::now();
        const auto pairs=newton::analytics::factorized_dense_join_group_sum_into(
            left,groups,values,right,0,frequency,counts,sums);
        const auto end=std::chrono::steady_clock::now();
        const auto ms=std::chrono::duration<double,std::milli>(end-start).count();
        std::cout << "{\"width\":"<<width<<",\"left_rows\":"<<left_n
                  <<",\"right_rows\":"<<right_n<<",\"pairs\":"<<pairs
                  <<",\"elapsed_ms\":"<<ms<<",\"scratch_bytes\":"
                  <<frequency.capacity()*sizeof(std::size_t)
                  <<",\"process_peak_rss_kib\":"<<rss_kib()
                  <<",\"counts\":";
        emit_array(counts);
        std::cout << ",\"sums\":";
        emit_array(sums);
        std::cout << "}\n";
        return 0;
    }catch(const std::exception& e) {
        std::cerr<<"factorized DuckDB screen failed: "<<e.what()<<"\n";
        return 2;
    }
}
