#include <newton/analytics/columnar_kernels.hpp>
#include <newton/columnar/views.hpp>
#include <newton/graph/csr_graph.hpp>
#include <newton/graph/shortest_path.hpp>
#include <newton/memory/mapped_file.hpp>
#include <newton/parallel/executor.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool good, const char* message) {
    if (!good) throw std::runtime_error(message);
}
template<class Ex, class F> void must_throw(F&& operation) {
    bool seen = false;
    try { operation(); } catch (const Ex&) { seen = true; }
    require(seen, "Expected exception was not thrown");
}
void graph_edges() {
    using namespace newton::graph;
    GraphBuilder builder(6);
    builder.add_edge(0, 1, 0);
    builder.add_edge(1, 2, 0);
    builder.add_edge(0, 2, 9);
    builder.add_edge(2, 3, std::numeric_limits<std::uint64_t>::max() - 8);
    builder.add_edge(3, 4, 10); // overflowing route must not wrap
    builder.add_edge(0, 4, 42);
    const auto g = builder.build();
    ShortestPathWorkspace workspace(g.node_count());
    for (int repetition = 0; repetition < 500; ++repetition) {
        for (auto algorithm : {ShortestPathAlgorithm::RadixHeap,
                               ShortestPathAlgorithm::BinaryHeap}) {
            require(workspace.query(g, 0, 2, algorithm) == 0,
                    "Zero-cost route lost");
            require(workspace.query(g, 0, 4, algorithm) == 42,
                    "Overflow produced a false cheaper route");
            require(!workspace.query(g, 4, 0, algorithm),
                    "Directed disconnected route found");
            require(workspace.query(g, 5, 5, algorithm) == 0,
                    "Identity distance not zero");
        }
    }
    must_throw<std::out_of_range>([&]{ (void)workspace.query(g, 0, 6); });
}
void executor_edges() {
    using newton::parallel::Executor;
    Executor single(0);
    require(single.size() == 1, "Zero-thread configuration did not select one worker");
    std::atomic<int> work{0};
    single.parallel_for(0, 0, 1, [&](auto,auto){ ++work; });
    require(work == 0, "Empty range invoked callback");
    auto nested = single.submit([&]{
        single.parallel_for(0, 53, 7, [&](std::size_t begin, std::size_t end){
            work += static_cast<int>(end - begin);
        });
        return work.load();
    });
    require(nested.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "Nested worker scheduling deadlocked");
    require(nested.get() == 53, "Nested worker yielded incomplete output");
    must_throw<std::invalid_argument>([&]{
        single.parallel_for(0, 1, 0, [](auto,auto){});
    });
    must_throw<std::invalid_argument>([&]{
        single.parallel_for(2, 1, 1, [](auto,auto){});
    });
}
void mapping_edges() {
    namespace fs = std::filesystem;
    using newton::memory::MappedFile;
    const auto folder = fs::temp_directory_path();
    const auto empty_path = folder / "newton-audit-empty-mapping.bin";
    const auto data_path = folder / "newton-audit-binary-mapping.bin";
    try {
        { std::ofstream empty(empty_path, std::ios::binary | std::ios::trunc); }
        {
            MappedFile empty(empty_path);
            require(empty.size() == 0 && empty.text().empty(),
                    "Empty mapped file not supported");
        }
        std::string data(8193, 'x');
        data[0] = '\0'; data[4095] = '\n'; data[4096] = '\r'; data.back() = '\xff';
        { std::ofstream file(data_path, std::ios::binary | std::ios::trunc);
          file.write(data.data(), static_cast<std::streamsize>(data.size())); }
        {
            MappedFile one(data_path);
            require(one.size() == data.size() && one.text() == data,
                    "Mapped binary contents differ");
            MappedFile two(empty_path);
            two = std::move(one);
            require(one.size() == 0 && one.text().empty() &&
                    two.size() == data.size() && two.text() == data,
                    "Move assignment lost or duplicated mapping ownership");
            two = std::move(two);
            require(two.text() == data, "Self move destroyed mapping");
        }
        fs::remove(empty_path);
        fs::remove(data_path);
    } catch (...) {
        std::error_code ignored;
        fs::remove(empty_path, ignored); fs::remove(data_path, ignored);
        throw;
    }
}
void nullable_edges() {
    using newton::columnar::ColumnView;
    using newton::columnar::ValidityView;
    using newton::analytics::select_greater_into;
    using newton::analytics::sum_where_greater;
    const std::array<std::int64_t, 11> values{0,1,2,3,4,5,6,7,8,9,10};
    const std::array<std::uint64_t, 11> weights{1,2,3,4,5,6,7,8,9,10,11};
    const std::array<std::uint8_t, 2> validity_bits{0b11010100,0b01101101};
    const ValidityView valid{validity_bits, 3};
    const ColumnView<std::int64_t> p(values, valid);
    const ColumnView<std::uint64_t> v(weights, valid);
    std::array<std::uint32_t, 11> selected{};
    std::vector<std::uint32_t> oracle;
    std::uint64_t oracle_count = 0, oracle_sum = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto bit = i + 3;
        const bool present = (validity_bits[bit / 8] >> (bit % 8)) & 1;
        if (present && values[i] > 4) {
            oracle.push_back(static_cast<std::uint32_t>(i));
            ++oracle_count;
            oracle_sum += weights[i];
        }
    }
    const auto count = select_greater_into(p, std::int64_t{4}, selected);
    const auto folded = sum_where_greater(p, v, std::int64_t{4});
    require(count == oracle.size() &&
            std::equal(oracle.begin(), oracle.end(), selected.begin()) &&
            folded.count == oracle_count && folded.sum == oracle_sum,
            "Offset bitmap null handling differs from independent oracle");
    must_throw<std::length_error>([&]{
        (void)ColumnView<std::int64_t>(values,
            ValidityView{std::span<const std::uint8_t>(validity_bits).first(1), 3});
    });
}
} // namespace
int main() {
    try {
        graph_edges(); executor_edges(); mapping_edges(); nullable_edges();
        std::cout << "Graph/executor/mapping/columnar audit boundaries: PASS\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
