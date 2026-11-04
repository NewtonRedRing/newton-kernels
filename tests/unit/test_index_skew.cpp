#include <newton/index/inverted_index.hpp>
#include <algorithm>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool ok) {
    if (!ok) throw std::runtime_error("adaptive postings differential regression");
}
int main() {
    using newton::index::InvertedIndex;
    using newton::index::IntersectionStrategy;
    {
        InvertedIndex idx;
        std::vector<std::uint32_t> expected;
        for (std::uint32_t i=0; i<5000; ++i) {
            const bool rare=(i % 113 == 0);
            const bool middle=(i % 3 == 0);
            idx.add_document(std::string("common ")
                + (rare?"rare ":"") + (middle?"middle ":"") + "other");
            if (rare) expected.push_back(i);
        }
        require(idx.search_all("RARE common") == expected);
        require(idx.search_all("common rare") == expected);
        require(idx.search_all("common rare", IntersectionStrategy::InputOrder) == expected);
        require(idx.search_all("rare rare") == expected);
        require(idx.search_all("rare nonexistent").empty());
        require(idx.search_all("nonexistent").empty());
        require(idx.search_phrase("common rare") == expected);
    }
    std::mt19937_64 rng(0x51928aec);
    for (int trial=0;trial<70;++trial) {
        InvertedIndex idx;
        std::vector<std::uint32_t> expected;
        const unsigned n=100+static_cast<unsigned>(rng()%900);
        const unsigned divisor=2+static_cast<unsigned>(rng()%128);
        for (std::uint32_t i=0;i<n;++i) {
            const bool rare=(rng()%divisor)==0;
            const bool mid=(rng()%4)==0;
            idx.add_document(std::string("common ")
                +(rare?"rare ":"")+(mid?"middle ":"")+"tail");
            if (rare) expected.push_back(i);
        }
        const auto smallest=idx.search_all("rare common");
        const auto input_order=idx.search_all("common rare",IntersectionStrategy::InputOrder);
        require(smallest==expected && input_order==expected);
        require(idx.search_all("common").size()==n);
    }
}
