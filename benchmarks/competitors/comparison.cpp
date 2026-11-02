// All benchmark adapters below are original work. External implementations are
// linked as dependencies, never vendored into the NewtonKernels source tree.
#include <newton/graph/csr_graph.hpp>
#include <newton/graph/shortest_path.hpp>
#include <newton/parsing/csv_parser.hpp>
#include <newton/simd/byte_scan.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#if defined(NEWTON_COMP_BENCH_BOOST)
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#endif
#if defined(NEWTON_COMP_BENCH_VINCE)
#include <csv.hpp>
#endif
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace {
using clock_type = std::chrono::steady_clock;
struct Rng {
    std::uint64_t state;
    std::uint64_t next() {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        return state;
    }
};
struct Options {
    std::string operation, implementation;
    std::uint64_t seed = 0x4f71ca3dULL;
    std::size_t size = 65536, queries = 64;
    int repetitions = 7;
};
std::uint64_t integer(const std::string& text) {
    std::size_t pos = 0;
    const auto value = std::stoull(text, &pos);
    if (pos != text.size()) throw std::invalid_argument("Invalid integer: " + text);
    return value;
}
Options parse_arguments(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) throw std::invalid_argument("Expected value for option");
        const std::string key(argv[i]), value(argv[i+1]);
        if (key == "--operation") o.operation = value;
        else if (key == "--implementation") o.implementation = value;
        else if (key == "--size") o.size = static_cast<std::size_t>(integer(value));
        else if (key == "--queries") o.queries = static_cast<std::size_t>(integer(value));
        else if (key == "--seed") o.seed = integer(value);
        else if (key == "--repetitions") o.repetitions = static_cast<int>(integer(value));
        else throw std::invalid_argument("Unknown option: " + key);
    }
    if (o.operation.empty() || o.implementation.empty() || o.size == 0 ||
        o.queries == 0 || o.repetitions < 3 || o.repetitions > 101)
        throw std::invalid_argument("Usage: --operation scan|graph|csv --implementation ... --size N --queries N --seed N --repetitions [3..101]");
    return o;
}
void fold(std::uint64_t& h, std::string_view s) {
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    h ^= 0xffU; h *= 1099511628211ULL;
}
void fold_uint(std::uint64_t& h, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) { h ^= (v & 0xffU); h *= 1099511628211ULL; v >>= 8; }
}
// Separate untimed input construction and reference evaluation from timing.
struct Fixture { std::uint64_t checksum = 0, count = 0; std::size_t bytes = 0; };
std::string make_scan(std::size_t n, std::uint64_t seed) {
    Rng rng{seed}; std::string data(n, 'a');
    for (auto& ch : data) ch = (rng.next() % 17 == 0) ? ',' : 'a';
    return data;
}
std::string make_csv(std::size_t n, std::uint64_t seed) {
    Rng rng{seed}; std::string data; data.reserve(n * 40);
    for (std::size_t i = 0; i < n; ++i) {
        data.append(std::to_string(i));
        data.append(",alpha,");
        if (i % 7 == 0) data.append("\"quoted, value\"");
        else if (i % 11 == 0) data.append("\"line1\nline2\"");
        else if (i % 13 == 0) data.append("\"escaped \"\"value\"\"\"");
        else data.append("plain");
        data.push_back(',');
        data.append(std::to_string(rng.next() % 100000));
        data.append((i % 2 == 0) ? "\r\n" : "\n");
    }
    return data;
}
struct EdgeInput { std::uint32_t from, to; std::uint64_t weight; };
struct GraphFixture {
    std::vector<EdgeInput> edges;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
};
GraphFixture make_graph(std::size_t nodes, std::size_t queries, std::uint64_t seed) {
    if (nodes > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("Graph too large for 32-bit node IDs");
    GraphFixture result; Rng rng{seed};
    result.edges.reserve(nodes * 6);
    for (std::size_t i = 0; i < nodes; ++i) {
        result.edges.push_back({static_cast<std::uint32_t>(i),
            static_cast<std::uint32_t>((i + 1) % nodes), 1 + rng.next() % 100});
        for (int e = 0; e < 5; ++e)
            result.edges.push_back({static_cast<std::uint32_t>(i),
                static_cast<std::uint32_t>(rng.next() % nodes), 1 + rng.next() % 100});
    }
    for (std::size_t i = 0; i < queries; ++i)
        result.pairs.emplace_back(rng.next() % nodes, rng.next() % nodes);
    return result;
}
#if defined(NEWTON_COMP_BENCH_BOOST)
using BoostGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
    boost::no_property, boost::property<boost::edge_weight_t, std::uint64_t>>;
struct ReachedTarget {};
struct StopOnTarget : boost::default_dijkstra_visitor {
    std::uint32_t target;
    template <typename Vertex, typename Graph> void examine_vertex(Vertex v, const Graph&) const {
        if (static_cast<std::uint32_t>(v) == target) throw ReachedTarget{};
    }
};
#endif
struct Prepared {
    std::string bytes;
    newton::graph::CsrGraph graph;
    std::optional<newton::graph::ShortestPathWorkspace> workspace;
#if defined(NEWTON_COMP_BENCH_BOOST)
    std::optional<BoostGraph> boost_graph;
#endif
    GraphFixture graph_fixture;
};
Prepared prepare(const Options& opts) {
    Prepared p;
    if (opts.operation == "scan") p.bytes = make_scan(opts.size, opts.seed);
    else if (opts.operation == "csv") p.bytes = make_csv(opts.size, opts.seed);
    else if (opts.operation == "graph") {
        p.graph_fixture = make_graph(opts.size, opts.queries, opts.seed);
        newton::graph::GraphBuilder builder(opts.size);
        for (const auto& e : p.graph_fixture.edges) builder.add_edge(e.from, e.to, e.weight);
        p.graph = builder.build();
        p.workspace.emplace(opts.size);
#if defined(NEWTON_COMP_BENCH_BOOST)
        p.boost_graph.emplace(opts.size);
        for (const auto& e : p.graph_fixture.edges)
            boost::add_edge(e.from, e.to, e.weight, *p.boost_graph);
#endif
    } else throw std::invalid_argument("Unknown operation");
    return p;
}
Fixture execute(const Options& o, Prepared& p) {
    Fixture result;
    result.checksum = 14695981039346656037ULL;
    if (o.operation == "scan") {
        std::uint64_t hits = 0;
        if (o.implementation == "newton")
            hits = newton::simd::count_byte(p.bytes, ',');
        else if (o.implementation == "stl")
            hits = static_cast<std::uint64_t>(std::count(p.bytes.begin(), p.bytes.end(), ','));
        else throw std::invalid_argument("Invalid scan implementation");
        fold_uint(result.checksum, hits);
        result.count = hits;
        result.bytes = p.bytes.size();
    } else if (o.operation == "csv") {
        auto consume = [&](const auto& row) {
            fold_uint(result.checksum, row.size());
            for (const auto& field : row) {
                if constexpr (std::is_same_v<std::decay_t<decltype(field)>, std::string>)
                    fold(result.checksum, field);
                else fold(result.checksum, field.template get<std::string>());
            }
            ++result.count;
        };
        if (o.implementation == "newton")
            (void)newton::parsing::CsvParser{}.parse(p.bytes, consume);
#if defined(NEWTON_COMP_BENCH_VINCE)
        else if (o.implementation == "vince") {
            csv::CSVFormat format;
            format.delimiter(',').no_header().threading(false);
            auto reader = csv::parse_unsafe(p.bytes, format);
            for (auto& row : reader) consume(row);
        }
#endif
        else throw std::invalid_argument("CSV implementation unavailable");
        result.bytes = p.bytes.size();
    } else if (o.operation == "graph") {
        if (o.implementation == "newton") {
            for (const auto& [s, t] : p.graph_fixture.pairs) {
                const auto d = p.workspace->query(p.graph, s, t);
                fold_uint(result.checksum, d.value_or(std::numeric_limits<std::uint64_t>::max()));
                result.count++;
            }
        }
#if defined(NEWTON_COMP_BENCH_BOOST)
        else if (o.implementation == "boost") {
            std::vector<std::uint64_t> distances(o.size);
            for (const auto& [s, t] : p.graph_fixture.pairs) {
                std::uint64_t d = std::numeric_limits<std::uint64_t>::max();
                try {
                    boost::dijkstra_shortest_paths(*p.boost_graph, s,
                        boost::distance_map(boost::make_iterator_property_map(
                            distances.begin(), get(boost::vertex_index, *p.boost_graph)))
                        .distance_inf(std::numeric_limits<std::uint64_t>::max())
                        .distance_zero(std::uint64_t{0})
                        .visitor(StopOnTarget{boost::default_dijkstra_visitor{}, t}));
                } catch (const ReachedTarget&) { }
                d = distances.at(t);
                fold_uint(result.checksum, d);
                result.count++;
            }
        }
#endif
        else throw std::invalid_argument("Graph implementation unavailable");
        result.bytes = p.graph_fixture.edges.size() * sizeof(EdgeInput);
    }
    return result;
}
std::uint64_t peak_rss_kib() {
#if defined(__unix__) || defined(__APPLE__)
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__)
        return static_cast<std::uint64_t>(usage.ru_maxrss) / 1024;
#else
        return static_cast<std::uint64_t>(usage.ru_maxrss);
#endif
    }
#endif
    return 0;
}
} // namespace
int main(int argc, char** argv) {
    try {
        const auto options = parse_arguments(argc, argv);
        auto prepared = prepare(options);
        const auto baseline = execute(options, prepared); // warmup and mandatory integrity check
        std::vector<double> times;
        for (int i = 0; i < options.repetitions; ++i) {
            const auto start = clock_type::now();
            const auto sample = execute(options, prepared);
            const auto end = clock_type::now();
            if (sample.checksum != baseline.checksum || sample.count != baseline.count)
                throw std::runtime_error("Nondeterministic result");
            times.push_back(std::chrono::duration<double, std::milli>(end-start).count());
        }
        std::sort(times.begin(), times.end());
        std::cout << std::fixed << std::setprecision(6)
                  << "{\"operation\":\"" << options.operation
                  << "\",\"implementation\":\"" << options.implementation
                  << "\",\"size\":" << options.size
                  << ",\"queries\":" << options.queries
                  << ",\"seed\":" << options.seed
                  << ",\"repetitions\":" << options.repetitions
                  << ",\"median_ms\":" << times[times.size()/2]
                  << ",\"minimum_ms\":" << times.front()
                  << ",\"maximum_ms\":" << times.back()
                  << ",\"checksum\":\"" << std::hex << baseline.checksum << std::dec
                  << "\",\"items\":" << baseline.count
                  << ",\"input_bytes\":" << baseline.bytes
                  << ",\"peak_process_rss_kib\":" << peak_rss_kib()
                  << "}" << std::endl;
    } catch(const std::exception& e) {
        std::cerr << "Comparison benchmark error: " << e.what() << '\n';
        return 2;
    }
}
