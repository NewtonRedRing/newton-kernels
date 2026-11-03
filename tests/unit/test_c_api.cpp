#include <newton/c_api/newton_c.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>

int main() {
    const std::array<std::uint8_t, 7> bytes{0, 5, 5, 0, 5, 3, 5};
    std::size_t found = 0;
    if (newton_count_byte(bytes.data(), bytes.size(), 5, &found) != 0 || found != 4)
        return 1;
    if (newton_count_byte(nullptr, 1, 5, &found) != NEWTON_STATUS_INVALID_ARGUMENT)
        return 2;
    if (newton_count_byte(nullptr, 0, 5, &found) != NEWTON_STATUS_OK || found != 0)
        return 3;
    const std::array<std::uint64_t, 5> vals{2, 5, 6, 8, 100};
    std::uint64_t count=0,sum=0;
    if (newton_sum_gt_u64(vals.data(), vals.size(), 5, &count, &sum) != 0 ||
        count != 3 || sum != 114)
        return 4;
    const std::array<std::uint64_t, 2> over{
        std::numeric_limits<std::uint64_t>::max(), 2};
    if (newton_sum_gt_u64(over.data(), over.size(), 0, &count, &sum) != NEWTON_STATUS_OVERFLOW ||
        count != 3 || sum != 114)
        return 5;
    std::uint64_t overlapping = 99;
    if (newton_sum_gt_u64(vals.data(), vals.size(), 5,
            &overlapping, &overlapping) != NEWTON_STATUS_INVALID_ARGUMENT ||
        overlapping != 99)
        return 6;
    std::cout << "C ABI smoke: PASS\n";
}
