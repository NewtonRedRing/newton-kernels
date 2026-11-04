#include <newton/index/hybrid_postings.hpp>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <exception>
#include <stdexcept>
#include <vector>
#include <array>
#include <cstdint>

namespace {
std::atomic<int> fail_after{-1};
// Initialized before injection; rethrowing avoids constructing a new exception
// while the global allocation override is deliberately failing.
std::exception_ptr prepared_bad_alloc;
void require(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}
template<class F>
bool failure_at(int offset, F&& operation) {
    fail_after.store(offset, std::memory_order_relaxed);
    bool failed = false;
    try {
        operation();
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    fail_after.store(-1, std::memory_order_relaxed);
    return failed;
}
} // namespace

// Allocation failures are injected only in this separate test executable.
void* operator new(std::size_t n) {
    const auto count = fail_after.load(std::memory_order_relaxed);
    if (count == 0) {
        // Disarm before throwing so exception construction cannot recursively fail.
        fail_after.store(-1, std::memory_order_relaxed);
        std::rethrow_exception(prepared_bad_alloc);
    }
    if (count > 0) fail_after.store(count - 1, std::memory_order_relaxed);
    if (void* ptr = std::malloc(n ? n : 1)) return ptr;
    throw std::bad_alloc();
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* ptr) noexcept { ::operator delete(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { ::operator delete(ptr); }

int main() {
    using newton::index::SelectiveTermIndex;
    try {
        prepared_bad_alloc = std::make_exception_ptr(std::bad_alloc{});
        // Distinguish an MSVC exception/allocator interception hang from an
        // index rollback bug before entering the full failure-point sweep.
        const bool smoke_failed = failure_at(0, [] {
            void* memory = ::operator new(16);
            ::operator delete(memory);
        });
        require(smoke_failed, "Injected global operator new did not fail");
        int query_failures = 0, document_failures = 0, finalize_failures = 0;
        bool query_success = false, document_success = false, finalize_success = false;
        for (int n = 0; n < 160; ++n) {
            SelectiveTermIndex idx;
            idx.add_query("alpha");
            const bool failed = failure_at(n, [&] {
                idx.add_query("beta gamma delta epsilon");
            });
            if (failed) {
                ++query_failures;
                require(idx.indexed_term_count() == 1,
                        "Partially registered query terms after allocation failure");
                idx.add_query("beta gamma delta epsilon");
            } else query_success = true;
            require(idx.indexed_term_count() == 5,
                    "Query registration lost a term");
            idx.add_document("alpha gamma alpha");
            idx.finalize();
            std::array<std::uint32_t, 2> out{};
            require(idx.search_all_into("alpha gamma", out) == 1 && out[0] == 0,
                    "Query registration changed results");
            if (!failed) break;
        }
        for (int n = 0; n < 160; ++n) {
            SelectiveTermIndex idx;
            idx.add_query("alpha beta gamma delta epsilon");
            require(idx.add_document("alpha beta") == 0, "Baseline document ID");
            const bool failed = failure_at(n, [&] {
                (void)idx.add_document(
                    "gamma delta epsilon beta gamma delta alpha");
            });
            if (failed) {
                ++document_failures;
                require(idx.document_count() == 1,
                        "Failed document insertion consumed an ID");
                require(idx.add_document("epsilon gamma") == 1,
                        "Failed insertion left partial postings or a skipped ID");
                idx.finalize();
                std::array<std::uint32_t, 3> out{};
                require(idx.search_all_into("beta gamma", out) == 0,
                        "Partial document postings leaked into AND results");
                require(idx.search_all_into("epsilon gamma", out) == 1 &&
                        out[0] == 1, "Recovery document cannot be found");
            } else {
                document_success = true;
                require(idx.document_count() == 2,
                        "Successful insertion lost document");
                idx.finalize();
                std::array<std::uint32_t, 3> out{};
                require(idx.search_all_into("gamma delta", out) == 1 &&
                        out[0] == 1, "Successful document not indexed");
            }
            if (!failed) break;
        }
        for (int n = 0; n < 160; ++n) {
            SelectiveTermIndex idx;
            idx.add_query("alpha beta gamma delta");
            idx.add_document("alpha beta gamma");
            idx.add_document("beta gamma delta");
            const bool failed = failure_at(n, [&] { idx.finalize(); });
            if (failed) {
                ++finalize_failures;
                bool guarded = false;
                std::array<std::uint32_t, 2> output{};
                try { (void)idx.search_all_into("beta", output); }
                catch (const std::logic_error&) { guarded = true; }
                require(guarded, "Failed finalization published partial postings");
                idx.finalize();
            } else finalize_success = true;
            std::array<std::uint32_t, 2> out{};
            require(idx.search_all_into("beta gamma", out) == 2 &&
                    out[0] == 0 && out[1] == 1, "Finalization changed matching docs");
            require(idx.search_all_into("alpha delta", out) == 0,
                    "Finalization changed intersection behavior");
            idx.finalize(); // Idempotent and allocation-free.
            if (!failed) break;
        }
        require(query_success && document_success && finalize_success &&
                query_failures >= 2 && document_failures >= 2 &&
                finalize_failures >= 2,
                "Allocation-failure campaign did not cover all transaction phases");
        std::cout << "Selective index fault injection: PASS\n";
    } catch (const std::exception& e) {
        fail_after.store(-1, std::memory_order_relaxed);
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
