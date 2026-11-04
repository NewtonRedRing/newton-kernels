#include <newton/analytics/adaptive_factorized_join.hpp>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

static void require(bool ok) {
    if (!ok) throw std::runtime_error("adaptive factorized join parity failure");
}
static void check(std::vector<std::uint64_t> left,
                  std::vector<std::uint32_t> groups,
                  std::vector<std::uint64_t> values,
                  std::vector<std::uint64_t> right,
                  std::size_t budget) {
    std::vector<std::uint64_t> expected_count(7), expected_sum(7);
    std::size_t pairs=0;
    for (std::size_t i=0;i<left.size();++i)
        for (const auto key:right)
            if (key==left[i]) {
                ++pairs;
                ++expected_count[groups[i]];
                expected_sum[groups[i]]+=values[i];
            }
    std::vector<std::uint64_t> counts(7,888), sums(7,999);
    auto result=newton::analytics::factorized_adaptive_join_group_sum_into(
        left,groups,values,right,counts,sums,budget);
    require(result.matching_pairs==pairs);
    require(counts==expected_count && sums==expected_sum);
    if (!right.empty() && !left.empty()) {
        if (budget==0)
            require(result.strategy==newton::analytics::AdaptiveJoinStrategy::SparseHash);
        else if (result.strategy==newton::analytics::AdaptiveJoinStrategy::DenseHistogram)
            require(result.dense_scratch_bytes<=budget);
    }
}
int main() {
    using namespace newton::analytics;
    check({},{},{},{},0);
    check({0,1,1,2},{0,1,1,2},{5,6,7,8},{1,1,2},0);
    check({0,1,1,2},{0,1,1,2},{5,6,7,8},{1,1,2},4096);
    const auto mx=std::numeric_limits<std::uint64_t>::max();
    check({mx-1,mx,0},{0,0,1},{1,2,3},{mx,mx,0},4096);
    check({mx-1,mx,0},{0,0,1},{1,2,3},{mx,mx,0},0);
    std::mt19937_64 rng(0x5ef7928a);
    for (int trial=0;trial<800;++trial) {
        std::vector<std::uint64_t> left,right,vals;
        std::vector<std::uint32_t> groups;
        const auto nleft=static_cast<std::size_t>(rng()%55);
        const auto nright=static_cast<std::size_t>(rng()%55);
        const std::uint64_t base=(trial%5==0) ? mx-1000 :
                                  (trial%5==1) ? 0 : 123456789;
        const auto width=static_cast<std::uint64_t>(1+rng()%50);
        for (std::size_t i=0;i<nleft;++i) {
            left.push_back(base+(rng()%width));
            groups.push_back(static_cast<std::uint32_t>(rng()%7));
            vals.push_back(rng()%100);
        }
        for (std::size_t i=0;i<nright;++i)
            right.push_back(base+(rng()%width));
        check(left,groups,vals,right,0);
        check(left,groups,vals,right,4096);
        check(left,groups,vals,right,32);
    }
    // Strict transactional outputs on overflow, including after some groups
    // would otherwise have been partially updated.
    {
        const std::vector<std::uint64_t> left{5,5},right{5,5},values{1,mx};
        const std::vector<std::uint32_t> groups{0,0};
        for (const auto budget : {std::size_t{0},std::size_t{4096}}) {
            std::vector<std::uint64_t> counts(1,45),sums(1,67);
            bool thrown=false;
            try {
                (void)factorized_adaptive_join_group_sum_into(
                    left,groups,values,right,counts,sums,budget);
            } catch (const std::overflow_error&) { thrown=true; }
            require(thrown && counts[0]==45 && sums[0]==67);
        }
    }
    {
        const std::vector<std::uint64_t> left{5},right{5},values{1};
        const std::vector<std::uint32_t> bad_group{1};
        std::vector<std::uint64_t> counts(1,45),sums(1,67);
        bool thrown=false;
        try {
            (void)factorized_adaptive_join_group_sum_into(
                left,bad_group,values,right,counts,sums);
        } catch(const std::out_of_range&) { thrown=true; }
        require(thrown && counts[0]==45 && sums[0]==67);
    }
}
