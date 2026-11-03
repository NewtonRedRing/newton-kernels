// Direct C++20 CSV -> exact decimal -> bounded grouped sums.
// No global context, implicit threads, JIT or hidden execution service.
#include <newton/analytics/dense_group.hpp>
#include <newton/parsing/ascii_numeric.hpp>
#include <newton/parsing/csv_fields.hpp>
#include <newton/parsing/csv_records.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>

int main() {
    constexpr std::string_view csv =
        "1,12.50\n"
        "2,3.25\n"
        "1,0.75\n"
        "0,10.00\n";
    std::array<std::uint32_t, 4> keys{};
    std::array<std::uint64_t, 4> values{};
    std::array<std::string_view, 2> fields{};
    std::array<char, 128> decoding{};
    std::size_t rows = 0;
    newton::parsing::for_each_csv_record(csv, [&](std::string_view raw) {
        const auto n = newton::parsing::decode_csv_fields_into(raw, fields, decoding);
        if (n != 2 || rows >= keys.size()) throw std::invalid_argument("Expected two fields");
        const auto key = newton::parsing::parse_ascii_uint64(fields[0]);
        const auto cents = newton::parsing::parse_decimal_cents(fields[1]);
        if (!key || *key >= 3 || !cents || *cents < 0)
            throw std::invalid_argument("Expected key 0..2 and nonnegative cents");
        keys[rows] = static_cast<std::uint32_t>(*key);
        values[rows] = static_cast<std::uint64_t>(*cents);
        ++rows;
    });
    std::array<std::size_t, 3> counts{};
    std::array<std::uint64_t, 3> sums{};
    newton::analytics::dense_group_count_sum_into(
        std::span(keys).first(rows), std::span(values).first(rows), counts, sums);
    for (std::size_t i = 0; i < counts.size(); ++i)
        std::cout << "key=" << i << " rows=" << counts[i]
                  << " exact_cents=" << sums[i] << '\n';
}
