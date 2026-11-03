#include <newton/graph/radix_heap.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

namespace {
std::atomic<int> fail_after{-1};
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
} // namespace

void* operator new(std::size_t n) {
    const auto count = fail_after.load(std::memory_order_relaxed);
    if (count == 0) {
        // A fault is one-shot: MSVC exception construction may allocate.
        fail_after.store(-1, std::memory_order_relaxed);
        throw std::bad_alloc();
    }
    if (count > 0) fail_after.store(count - 1, std::memory_order_relaxed);
    if (void* ptr = std::malloc(n ? n : 1)) return ptr;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

int main() {
    try {
        const std::array<std::uint64_t, 7> keys{200, 200, 201, 204, 205, 300, 302};
        int exceptions = 0;
        bool successful = false;
        for (int n = 0; n < 128; ++n) {
            newton::graph::RadixHeap heap;
            for (std::size_t i = 0; i < keys.size(); ++i)
                heap.push(keys[i], static_cast<std::uint32_t>(i));
            fail_after.store(n, std::memory_order_relaxed);
            bool failed = false;
            newton::graph::RadixHeap::Entry first{};
            try { first = heap.pop(); }
            catch (const std::bad_alloc&) { failed = true; }
            fail_after.store(-1, std::memory_order_relaxed);
            if (failed) {
                ++exceptions;
                require(heap.size() == keys.size(),
                        "A failed re-bucketing discarded heap entries");
            } else {
                successful = true;
                require(first.first == 200, "First heap key incorrect");
                require(heap.size() + 1 == keys.size(),
                        "Successful pop has incorrect size");
            }
            std::vector<std::uint64_t> observed;
            if (!failed) observed.push_back(first.first);
            while (!heap.empty()) observed.push_back(heap.pop().first);
            require(std::is_sorted(observed.begin(), observed.end()),
                    "Exception recovery violated heap ordering");
            require(observed == std::vector<std::uint64_t>(keys.begin(), keys.end()),
                    "Exception recovery lost or duplicated heap entries");
            if (!failed) break;
        }
        require(exceptions >= 1 && successful,
                "Did not cover radix redistribution allocation failures");
        std::cout << "RadixHeap exception recovery: PASS\n";
    } catch (const std::exception& e) {
        fail_after.store(-1, std::memory_order_relaxed);
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
