#include <newton/c_api/newton_c.h>
#include <newton/simd/byte_scan.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

extern "C" int newton_count_byte(
    const std::uint8_t* data, std::size_t size,
    std::uint8_t needle, std::size_t* count) {
    if (count == nullptr || (size != 0 && data == nullptr))
        return NEWTON_STATUS_INVALID_ARGUMENT;
    const auto bytes = size == 0 ? std::string_view{} :
        std::string_view(reinterpret_cast<const char*>(data), size);
    *count = newton::simd::count_byte(bytes, static_cast<char>(needle));
    return NEWTON_STATUS_OK;
}
extern "C" int newton_sum_gt_u64(
    const std::uint64_t* values, std::size_t size, std::uint64_t threshold,
    std::uint64_t* count, std::uint64_t* sum) {
    if (count == nullptr || sum == nullptr || (size != 0 && values == nullptr))
        return NEWTON_STATUS_INVALID_ARGUMENT;
    std::uint64_t local_count = 0, local_sum = 0;
    for (std::size_t i = 0; i < size; ++i) {
        if (values[i] <= threshold) continue;
        if (local_count == std::numeric_limits<std::uint64_t>::max() ||
            values[i] > std::numeric_limits<std::uint64_t>::max() - local_sum)
            return NEWTON_STATUS_OVERFLOW;
        ++local_count;
        local_sum += values[i];
    }
    *count = local_count;
    *sum = local_sum;
    return NEWTON_STATUS_OK;
}
