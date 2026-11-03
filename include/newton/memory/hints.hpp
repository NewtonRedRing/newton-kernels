#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace newton::memory {
// Explicit opt-in OS advice. Never changes global system policy or spawns
// threads. Platform may reject hints; callers must handle false.
enum class MemoryHint { Sequential, Random, WillNeed, DontNeed, HugePages };
inline bool advise_bytes(std::span<const std::byte> bytes, MemoryHint hint) noexcept {
#if defined(__linux__)
    if (bytes.empty()) return true;
    if (!bytes.data()) return false;
    const auto page = ::sysconf(_SC_PAGESIZE);
    if (page <= 0) return false;
    const auto size = static_cast<std::uintptr_t>(page);
    const auto start = reinterpret_cast<std::uintptr_t>(bytes.data());
    // MADV_DONTNEED can discard modifications of OTHER allocations sharing
    // the same page. Never round such a request outside the caller's span.
    if (hint == MemoryHint::DontNeed &&
        (start % size != 0 || bytes.size() % size != 0)) return false;
    if (bytes.size() > std::numeric_limits<std::uintptr_t>::max() - start)
        return false;
    const auto low = start - start % size;
    const auto end = start + bytes.size();
    const auto extra = (size - end % size) % size;
    if (extra > std::numeric_limits<std::uintptr_t>::max() - end) return false;
    int kind = 0;
    switch (hint) {
        case MemoryHint::Sequential: kind = MADV_SEQUENTIAL; break;
        case MemoryHint::Random: kind = MADV_RANDOM; break;
        case MemoryHint::WillNeed: kind = MADV_WILLNEED; break;
        case MemoryHint::DontNeed: kind = MADV_DONTNEED; break;
#ifdef MADV_HUGEPAGE
        case MemoryHint::HugePages: kind = MADV_HUGEPAGE; break;
#else
        case MemoryHint::HugePages: return false;
#endif
    }
    return ::madvise(reinterpret_cast<void*>(low), end + extra - low, kind) == 0;
#else
    (void)bytes;
    (void)hint;
    return false;
#endif
}
} // namespace newton::memory
