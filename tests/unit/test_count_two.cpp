#include <newton/simd/count_two.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

static void require(bool x) {
    if (!x) throw std::runtime_error("two-byte SIMD count regression");
}

int main() {
    using newton::simd::count_either_byte;
    require(count_either_byte({}, '\0', ',') == 0);
    require(count_either_byte(std::string_view(",,\0", 3), ',', ',') == 2);
    require(count_either_byte(std::string_view(",,\0", 3), '\0', ',') == 3);
    std::mt19937_64 rng(0x551abe227e);
    for (auto length : {0u, 1u, 2u, 15u, 16u, 31u, 32u, 33u, 63u,
                        64u, 65u, 127u, 128u, 129u, 1024u, 4097u}) {
        for (int round = 0; round < 70; ++round) {
            std::vector<char> bytes(length + 80);
            for (auto& c : bytes) c = static_cast<char>(rng() & 255);
            const auto a = static_cast<char>(rng() & 255);
            const auto b = round % 7 ? static_cast<char>(rng() & 255) : a;
            for (auto offset : {0u, 1u, 7u, 31u, 63u}) {
                const std::string_view view(bytes.data() + offset, length);
                const auto expected = static_cast<std::size_t>(std::count_if(
                    view.begin(), view.end(), [a,b](char c) {
                        return c == a || c == b;
                    }));
                require(count_either_byte(view, a, b) == expected);
            }
        }
    }
    // Fully matching large vector and unmapped-unrelated high-bit bytes.
    const std::string all(8192, static_cast<char>(0xFF));
    require(count_either_byte(all, static_cast<char>(0xFF), '\n') == all.size());
    require(count_either_byte(all, ',', '\n') == 0);
}
