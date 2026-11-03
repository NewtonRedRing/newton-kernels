#ifndef NEWTON_KERNELS_C_API_H
#define NEWTON_KERNELS_C_API_H
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
# if defined(NEWTON_C_API_BUILD)
#  define NEWTON_C_API __declspec(dllexport)
# else
#  define NEWTON_C_API __declspec(dllimport)
# endif
#else
# define NEWTON_C_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ABI v1. No global init/shutdown or hidden workers. Never throws across C.
// Status 0 success, 1 invalid argument, 2 integer overflow.
typedef enum newton_status {
    NEWTON_STATUS_OK = 0,
    NEWTON_STATUS_INVALID_ARGUMENT = 1,
    NEWTON_STATUS_OVERFLOW = 2
} newton_status;

NEWTON_C_API int newton_count_byte(
    const uint8_t* data, size_t size, uint8_t needle, size_t* count);
// count and sum must be distinct output objects. Neither is changed on
// argument validation failure or accumulation overflow.
NEWTON_C_API int newton_sum_gt_u64(
    const uint64_t* values, size_t size, uint64_t threshold,
    uint64_t* count, uint64_t* sum);

#ifdef __cplusplus
}
#endif
#endif
