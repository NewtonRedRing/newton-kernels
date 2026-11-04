#include <newton/analytics/factorized_dense_join.hpp>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

static void require(bool x) {
    if (!x) throw std::runtime_error("factorized join differential failure");
}
static void verify(const std::vector<std::uint64_t>& left,
                   const std::vector<std::uint32_t>& groups,
                   const std::vector<std::uint64_t>& values,
                   const std::vector<std::uint64_t>& right,
                   std::uint64_t base, std::size_t width,
                   std::size_t group_count) {
    std::vector<std::size_t> frequency(width);
    std::vector<std::uint64_t> counts(group_count), sums(group_count);
    std::vector<std::uint64_t> expected_counts(group_count), expected_sums(group_count);
    std::size_t expected_pairs = 0;
    for (std::size_t i=0; i<left.size(); ++i)
        for (const auto key : right)
            if (left[i] == key) {
                ++expected_pairs;
                ++expected_counts[groups[i]];
                expected_sums[groups[i]] += values[i];
            }
    const auto actual = newton::analytics::factorized_dense_join_group_sum_into(
        left, groups, values, right, base, frequency, counts, sums);
    require(actual == expected_pairs);
    require(counts == expected_counts && sums == expected_sums);
}
int main() {
    verify({}, {}, {}, {}, 0, 1, 1);
    verify({8,9,9,10,11}, {0,0,1,1,0}, {2,4,5,8,9},
           {9,9,10}, 9, 2, 2);
    const auto mx = std::numeric_limits<std::uint64_t>::max();
    verify({mx-3,mx-2,mx-1,mx}, {0,0,1,1}, {1,2,3,4},
           {mx-2,mx,mx}, mx-2, 3, 2);

    std::mt19937_64 rng(0x448526cb);
    for (auto width : {1u,2u,3u,16u,64u,1024u})
        for (int trial=0;trial<200;++trial) {
            std::vector<std::uint64_t> left, right, values;
            std::vector<std::uint32_t> groups;
            for (int i=0,n=static_cast<int>(rng()%65);i<n;++i) {
                left.push_back(27 + rng()%(width+2));
                groups.push_back(static_cast<std::uint32_t>(rng()%7));
                values.push_back(rng()%300);
            }
            for (int i=0,n=static_cast<int>(rng()%65);i<n;++i)
                right.push_back(28 + rng()%width);
            verify(left,groups,values,right,28,width,7);
        }
    std::vector<std::size_t> frequency(2,99);
    std::vector<std::uint64_t> counts(2,88), sums(2,77);
    const std::vector<std::uint64_t> left{8}, right{9}, values{1};
    const std::vector<std::uint32_t> groups{0};
    bool failed = false;
    try {
        const std::vector<std::uint64_t> bad_right{11};
        (void)newton::analytics::factorized_dense_join_group_sum_into(
            left,groups,values,bad_right,8,frequency,counts,sums);
    } catch (const std::out_of_range&) { failed = true; }
    require(failed && frequency[0]==99 && counts[0]==88 && sums[0]==77);
    failed = false;
    try {
        (void)newton::analytics::factorized_dense_join_group_sum_into(
            left,groups,values,right,8,{},counts,sums);
    } catch (const std::length_error&) { failed = true; }
    require(failed);
    failed = false;
    try {
        const std::vector<std::uint64_t> matching{8,8};
        const std::vector<std::uint64_t> huge{mx};
        (void)newton::analytics::factorized_dense_join_group_sum_into(
            left,groups,huge,matching,8,frequency,counts,sums);
    } catch (const std::overflow_error&) { failed = true; }
    require(failed);
}
