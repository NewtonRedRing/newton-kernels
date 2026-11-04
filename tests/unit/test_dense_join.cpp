#include <newton/analytics/dense_join.hpp>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using Pair = std::pair<std::size_t, std::size_t>;

static void require(bool condition) {
    if (!condition) throw std::runtime_error("Dense join differential test failed");
}

static void verify(std::vector<std::uint64_t> left,
                   std::vector<std::uint64_t> right,
                   std::uint64_t base, std::size_t width) {
    std::vector<std::size_t> heads(width, 0), next(right.size(), 0);
    std::vector<Pair> actual, expected;
    for (std::size_t i = 0; i < left.size(); ++i)
        for (std::size_t j = 0; j < right.size(); ++j)
            if (left[i] == right[j]) expected.emplace_back(i, j);
    const auto count = newton::analytics::dense_join_into(
        left, right, base, heads, next,
        [&](std::size_t i, std::size_t j) { actual.emplace_back(i, j); });
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    require(actual == expected);
    require(count == expected.size());
}

int main() {
    verify({}, {}, 0, 1);
    verify({0, 1, 1, 2, 3}, {1, 1, 3}, 1, 3);
    verify({100, 100, 101, 103}, {100, 100, 100, 101}, 100, 3);
    const auto max = std::numeric_limits<std::uint64_t>::max();
    verify({max-3, max-2, max-1, max}, {max-2, max, max}, max-2, 3);

    {
        std::vector<std::size_t> h(2), n(2);
        std::vector<std::uint64_t> l = {8}, r = {8, 11};
        bool called = false;
        try {
            newton::analytics::dense_join_into(l, r, 8, h, n,
                [&](auto, auto) { called = true; });
            require(false);
        } catch (const std::out_of_range&) { require(!called); }
        require(h == std::vector<std::size_t>({0, 0}));
        try {
            newton::analytics::dense_join_into(l, r, 8, h, std::span<std::size_t>(n).first(1),
                [&](auto, auto) { called = true; });
            require(false);
        } catch (const std::length_error&) { require(!called); }
        try {
            newton::analytics::dense_join_into(l, r, 8, std::span<std::size_t>(), n,
                [&](auto, auto) { called = true; });
            require(false);
        } catch (const std::length_error&) { require(!called); }
    }

    std::mt19937_64 rng(0xa4385e);
    for (std::size_t width : {1u, 2u, 3u, 16u, 63u, 256u, 1024u}) {
        for (unsigned trial = 0; trial < 200; ++trial) {
            const std::uint64_t base = 17;
            std::vector<std::uint64_t> left, right;
            const auto lsize = static_cast<std::size_t>(rng() % 71);
            const auto rsize = static_cast<std::size_t>(rng() % 71);
            for (std::size_t i = 0; i < lsize; ++i)
                left.push_back(base - 1 + (rng() % (width + 2)));
            for (std::size_t i = 0; i < rsize; ++i)
                right.push_back(base + (rng() % width));
            verify(std::move(left), std::move(right), base, width);
        }
    }
}
