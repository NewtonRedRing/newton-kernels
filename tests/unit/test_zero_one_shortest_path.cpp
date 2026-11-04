#include <newton/graph/csr_graph.hpp>
#include <newton/graph/shortest_path.hpp>
#include <newton/graph/zero_one_shortest_path.hpp>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>

static void require(bool x) {
    if (!x) throw std::runtime_error("0-1 BFS differential regression");
}

int main() {
    using namespace newton::graph;
    {
        GraphBuilder b(0);
        const auto g = b.build();
        ZeroOneShortestPathWorkspace zero_one(g);
        try { (void)zero_one.query(0, 0); require(false); }
        catch (const std::out_of_range&) {}
    }
    {
        GraphBuilder b(4);
        b.add_edge(0, 1, 0);
        b.add_edge(1, 2, 0);
        b.add_edge(0, 2, 1);
        const auto g = b.build();
        ZeroOneShortestPathWorkspace zero_one(g);
        require(zero_one.query(0, 2) == std::optional<std::uint64_t>{0});
        require(zero_one.query(0, 3) == std::nullopt);
        require(zero_one.query(3, 3) == std::optional<std::uint64_t>{0});
        try { (void)zero_one.query(4, 0); require(false); }
        catch (const std::out_of_range&) {}
    }
    {
        GraphBuilder b(2);
        b.add_edge(0, 1, 2);
        const auto g = b.build();
        try { ZeroOneShortestPathWorkspace bad(g); require(false); }
        catch (const std::invalid_argument&) {}
    }
    std::mt19937_64 rng(0x1874ca1da5);
    for (int trial = 0; trial < 240; ++trial) {
        const std::uint32_t nodes = 2 + static_cast<std::uint32_t>(rng() % 93);
        GraphBuilder b(nodes);
        const int edges = 1 + static_cast<int>(rng() % (nodes * 7));
        for (int i = 0; i < edges; ++i)
            b.add_edge(static_cast<std::uint32_t>(rng() % nodes),
                       static_cast<std::uint32_t>(rng() % nodes),
                       static_cast<std::uint64_t>(rng() % 2));
        const auto graph = b.build();
        ZeroOneShortestPathWorkspace zero_one(graph);
        ShortestPathWorkspace radix(nodes);
        for (int query = 0; query < 20; ++query) {
            const auto src = static_cast<std::uint32_t>(rng() % nodes);
            const auto dst = static_cast<std::uint32_t>(rng() % nodes);
            require(zero_one.query(src, dst) == radix.query(graph, src, dst));
            // Verify repeated-query scratch reuse without leaking old distances.
            require(zero_one.query(src, dst) == radix.query(graph, src, dst));
        }
    }
}
