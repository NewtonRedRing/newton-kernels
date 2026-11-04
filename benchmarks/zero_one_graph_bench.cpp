#include <newton/graph/csr_graph.hpp>
#include <newton/graph/shortest_path.hpp>
#include <newton/graph/zero_one_shortest_path.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

static double median(std::vector<double> x) {
    std::sort(x.begin(), x.end());
    return (x[(x.size()-1)/2] + x[x.size()/2]) / 2;
}
int main() {
    constexpr std::uint32_t nodes=8000, edges=48000;
    constexpr int queries=64;
    for (const auto one_pct : {10u, 50u, 90u}) {
        std::mt19937_64 rng(0x503621fa + one_pct);
        newton::graph::GraphBuilder builder(nodes);
        for (std::uint32_t i=0; i<edges; ++i)
            builder.add_edge(static_cast<std::uint32_t>(rng()%nodes),
                             static_cast<std::uint32_t>(rng()%nodes),
                             rng()%100 < one_pct ? 1 : 0);
        const auto graph = builder.build();
        std::vector<std::pair<std::uint32_t,std::uint32_t>> requests;
        for (int i=0; i<queries; ++i)
            requests.emplace_back(static_cast<std::uint32_t>(rng()%nodes),
                                  static_cast<std::uint32_t>(rng()%nodes));
        newton::graph::ShortestPathWorkspace radix(nodes);
        newton::graph::ZeroOneShortestPathWorkspace zero_one(graph);
        for (const auto& [s,t]: requests)
            if (radix.query(graph,s,t)!=zero_one.query(s,t))
                throw std::runtime_error("0-1 BFS did not match general Dijkstra");
        volatile std::uint64_t sink=0;
        std::vector<double> radix_ms,zero_one_ms;
        auto measure = [&](bool use_radix) {
            const auto start=std::chrono::steady_clock::now();
            std::uint64_t sum=0;
            for (auto [s,t]:requests) {
                const auto result = use_radix ? radix.query(graph,s,t) : zero_one.query(s,t);
                sum += result.value_or(0);
            }
            const auto end=std::chrono::steady_clock::now();
            sink = sink ^ sum;
            (use_radix?radix_ms:zero_one_ms).push_back(
                std::chrono::duration<double,std::milli>(end-start).count());
        };
        for (int r=0;r<9;++r) {
            if (r%2) {measure(true);measure(false);}
            else {measure(false);measure(true);}
        }
        std::cout << std::fixed << std::setprecision(5)
                  << "nodes=" << nodes << " edges=" << edges
                  << " queries=" << queries << " weight1_pct=" << one_pct
                  << " zero_one_ms=" << median(zero_one_ms)
                  << " radix_ms=" << median(radix_ms)
                  << " radix/zero_one=" << median(radix_ms)/median(zero_one_ms)
                  << " checksum=" << sink << '\n';
    }
}
