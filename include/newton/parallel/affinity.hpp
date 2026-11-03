#pragma once
#include <cstddef>
#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace newton::parallel {
// Pins only the CALLING thread, never another application's threads.
// OS-specific, opt-in, and nonportable; returns false when unsupported.
inline bool pin_current_thread(unsigned cpu) noexcept {
#if defined(__linux__)
    if (cpu >= CPU_SETSIZE) return false;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    return ::pthread_setaffinity_np(::pthread_self(), sizeof(mask), &mask) == 0;
#elif defined(_WIN32)
    if (cpu >= sizeof(DWORD_PTR) * 8) return false;
    const DWORD_PTR mask = DWORD_PTR{1} << cpu;
    return ::SetThreadAffinityMask(::GetCurrentThread(), mask) != 0;
#else
    (void)cpu;
    return false;
#endif
}
inline void prefetch_read(const void* pointer, int locality = 3) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    switch (locality) {
        case 0: __builtin_prefetch(pointer, 0, 0); break;
        case 1: __builtin_prefetch(pointer, 0, 1); break;
        case 2: __builtin_prefetch(pointer, 0, 2); break;
        default: __builtin_prefetch(pointer, 0, 3); break;
    }
#else
    (void)pointer;
    (void)locality;
#endif
}
} // namespace newton::parallel
