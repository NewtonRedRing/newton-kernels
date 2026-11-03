#include <newton/index/inverted_index.hpp>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

// Dedicated test executable: deterministic global allocation-failure injection.
// This is deliberately NOT linked into the production library or other tests.
namespace {
std::atomic<int> failures_after{-1};
void check(bool yes, const char* why) {
    if (!yes) throw std::runtime_error(why);
}
} // namespace

void* operator new(std::size_t size) {
    const auto index = failures_after.load(std::memory_order_relaxed);
    if (index >= 0) {
        if (index == 0) {
            // Prevent recursive injected failures in exception machinery.
            failures_after.store(-1, std::memory_order_relaxed);
            std::fprintf(stderr, "index_faults allocator throw offset 0 size=%zu\n", size);
            std::fflush(stderr);
            throw std::bad_alloc();
        }
        failures_after.store(index - 1, std::memory_order_relaxed);
    }
    if (void* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc();
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* ptr) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { ::operator delete(ptr); }

int main() {
    try {
        int injected_failures = 0;
        bool observed_success = false;
        for (int fail_at = 0; fail_at < 200; ++fail_at) {
            newton::index::InvertedIndex idx;
            check(idx.add_document("old beta old") == 0, "Initial document ID");
            std::fprintf(stderr, "index_faults injecting offset=%d\n", fail_at);
            std::fflush(stderr);
            failures_after.store(fail_at, std::memory_order_relaxed);
            bool failed = false;
            std::uint32_t result = 0;
            try {
                result = idx.add_document("alpha beta alpha gamma beta delta alpha");
            } catch (const std::bad_alloc&) {
                std::fprintf(stderr, "index_faults caught bad_alloc offset=%d\n", fail_at);
                std::fflush(stderr);
                failed = true;
            }
            failures_after.store(-1, std::memory_order_relaxed);
            if (failed) {
                ++injected_failures;
                check(idx.document_count() == 1, "Failed insertion consumed document ID");
                check(idx.search_all("alpha").empty(), "New term survived failed insertion");
                check(idx.search_all("gamma").empty(), "New gamma term survived failed insertion");
                check(idx.search_all("delta").empty(), "New delta term survived failed insertion");
                check(idx.search_all("beta") == std::vector<std::uint32_t>{0},
                      "Existing term gained partial document ID");
                check(idx.search_phrase("old beta old") == std::vector<std::uint32_t>{0},
                      "Original phrase changed after failed insertion");
                check(idx.add_document("recovered beta") == 1,
                      "Recovery insertion reused the wrong document ID");
                check(idx.search_all("recovered") == std::vector<std::uint32_t>{1},
                      "Recovery document not queryable");
            } else {
                observed_success = true;
                check(result == 1 && idx.document_count() == 2,
                      "Successful insertion has wrong document ID");
                check(idx.search_phrase("alpha beta alpha") ==
                      std::vector<std::uint32_t>{1}, "Successful phrase result changed");
                check(idx.search_all("old beta") == std::vector<std::uint32_t>{0},
                      "Existing document changed after successful insertion");
                break;
            }
        }
        check(injected_failures >= 6 && observed_success,
              "Allocation failure coverage did not reach successful insertion");
        std::cout << "Inverted index rollback injection: PASS ("
                  << injected_failures << " failure points)\\n";
    } catch (const std::exception& e) {
        failures_after.store(-1, std::memory_order_relaxed);
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
