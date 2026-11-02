#include <newton/graph/csr_graph.hpp>
#include <newton/graph/shortest_path.hpp>
#include <iostream>
#include <string>
int main() {
    newton::graph::GraphBuilder builder(3);
    builder.add_edge(0, 1, 3);
    builder.add_edge(1, 2, 4);
    builder.add_edge(0, 2, 9);
    auto answer = newton::graph::shortest_path(builder.build(), 0, 2);
    std::cout << "distance: " << (answer ? std::to_string(*answer) : "unreachable") << '\n';
}
