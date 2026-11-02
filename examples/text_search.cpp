#include <newton/index/inverted_index.hpp>
#include <iostream>
int main() {
    newton::index::InvertedIndex index;
    index.add_document("C++ high performance computing");
    index.add_document("Portable computing libraries");
    for (auto id : index.search_all("computing")) std::cout << "match: " << id << '\n';
}
