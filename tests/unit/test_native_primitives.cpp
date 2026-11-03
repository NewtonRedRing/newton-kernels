#include <newton/analytics/dense_group.hpp>
#include <newton/analytics/percentile.hpp>
#include <newton/parsing/csv_records.hpp>
#include <newton/parsing/csv_parser.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool value, const char* what) {
    if (!value) throw std::runtime_error(what);
}
template<class Exception, class Function>
void expect_throw(Function&& function, const char* what) {
    bool caught = false;
    try { function(); } catch (const Exception&) { caught = true; }
    require(caught, what);
}
void test_dense() {
    std::mt19937_64 rng{0xcafe5eed};
    for (unsigned seed = 0; seed < 65; ++seed) {
        const std::size_t domain = 1 + seed % 43;
        const std::size_t size = seed * 609;
        std::vector<std::uint32_t> keys(size);
        std::vector<std::uint64_t> values(size);
        std::vector<std::size_t> actual_counts(domain), expected_counts(domain);
        std::vector<std::uint64_t> actual_sums(domain), expected_sums(domain);
        for (std::size_t i = 0; i < size; ++i) {
            keys[i] = static_cast<std::uint32_t>(rng() % domain);
            values[i] = rng() % 10000;
            ++expected_counts[keys[i]];
            expected_sums[keys[i]] += values[i];
        }
        newton::analytics::dense_group_count_sum_into(keys, values, actual_counts, actual_sums);
        require(actual_counts == expected_counts && actual_sums == expected_sums,
                "dense grouped count/sum mismatch");
        std::fill(actual_counts.begin(), actual_counts.end(), 999);
        newton::analytics::dense_group_count_into(keys, actual_counts);
        require(actual_counts == expected_counts, "dense grouped count-only mismatch");
    }
    std::array<std::size_t, 2> counts{};
    std::array<std::uint64_t, 2> sums{};
    const std::array<std::uint32_t, 1> invalid_key{2};
    expect_throw<std::out_of_range>([&] {
        newton::analytics::dense_group_count_into(invalid_key, counts);
    }, "out-of-range group key accepted");
    const std::array<std::uint32_t, 2> keys{0, 0};
    const std::array<std::uint64_t, 2> overflow{std::numeric_limits<std::uint64_t>::max(), 1};
    expect_throw<std::overflow_error>([&] {
        newton::analytics::dense_group_count_sum_into(keys, overflow, counts, sums);
    }, "overflowing group sum accepted");
    expect_throw<std::invalid_argument>([&] {
        newton::analytics::dense_group_count_sum_into(keys, overflow, counts,
            std::span<std::uint64_t>(sums).first(1));
    }, "wrong output size accepted");
}

void test_percentiles() {
    using newton::analytics::percentile_nearest_rank;
    using newton::analytics::percentiles_nearest_rank_bounded_into;
    const std::array<double, 10> qs{0.0, 0.001, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99, 1.0};
    std::mt19937_64 rng{0x4371feed};
    for (unsigned seed = 0; seed < 65; ++seed) {
        const std::size_t domain = 1 + seed % 131;
        const std::size_t size = 1 + seed * 117;
        const std::uint64_t base = seed % 2 == 0 ? 0
            : std::numeric_limits<std::uint64_t>::max() - 256;
        std::vector<std::uint64_t> values(size);
        for (auto& value : values) value = base + (rng() % domain);
        const auto copy = values;
        std::vector<std::size_t> scratch(domain);
        std::array<std::uint64_t, qs.size()> output{};
        percentiles_nearest_rank_bounded_into(values, base, scratch, qs, output);
        for (std::size_t q = 0; q < qs.size(); ++q)
            require(output[q] == percentile_nearest_rank(values, qs[q]),
                    "exact bounded percentile differs from selection");
        require(values == copy && scratch.back() == values.size(),
                "bounded percentile input mutation or histogram error");
    }
    std::array<std::uint64_t, 1> one{3};
    std::array<std::size_t, 5> scratch{};
    std::array<std::uint64_t, 1> result{};
    const std::array<double, 1> valid{0.5}, invalid{-0.2};
    expect_throw<std::invalid_argument>([&] {
        percentiles_nearest_rank_bounded_into(one, 0, scratch, invalid, result);
    }, "bad percentile accepted");
    expect_throw<std::out_of_range>([&] {
        percentiles_nearest_rank_bounded_into(one, 4, scratch, valid, result);
    }, "out-of-domain input accepted");
    expect_throw<std::invalid_argument>([&] {
        percentiles_nearest_rank_bounded_into(
            one, std::numeric_limits<std::uint64_t>::max(), scratch, valid, result);
    }, "overflowing histogram domain accepted");
}
std::string csv_field(std::string_view value, bool force_quote) {
    if (!force_quote && value.find_first_of(",\"\r\n") == std::string_view::npos)
        return std::string(value);
    std::string encoded{"\""};
    for (char c : value) {
        if (c == '"') encoded.push_back('"');
        encoded.push_back(c);
    }
    encoded.push_back('"');
    return encoded;
}
void test_csv_records() {
    using newton::parsing::for_each_csv_record;
    std::string input = "a,b\r\n\"hello,\nworld\",42\n\"a\"\"b\",x\r,\n";
    std::vector<std::string> segments;
    require(for_each_csv_record(input, [&](std::string_view record) {
        segments.emplace_back(record);
    }) == 4, "CSV record count mismatch");
    require(segments[1] == "\"hello,\nworld\",42" &&
            segments[2] == "\"a\"\"b\",x", "quoted delimiters split a record");
    require(for_each_csv_record("", [](std::string_view){}) == 0,
            "empty CSV should produce no records");
    std::mt19937 rng{0xcafe2026};
    constexpr std::string_view alphabet = "xyz02,\"\n\r";
    newton::parsing::CsvParser parser;
    for (int trial = 0; trial < 100; ++trial) {
        std::vector<std::vector<std::string>> expected;
        std::string text;
        const int rows = 1 + trial % 24;
        for (int r = 0; r < rows; ++r) {
            std::vector<std::string> row;
            const int fields = 1 + static_cast<int>(rng() % 5);
            for (int f = 0; f < fields; ++f) {
                std::string value;
                const auto len = rng() % 9;
                for (unsigned i = 0; i < len; ++i)
                    value.push_back(alphabet[rng() % alphabet.size()]);
                if (f) text.push_back(',');
                text += csv_field(value, rng() % 3 == 0);
                row.push_back(std::move(value));
            }
            expected.push_back(std::move(row));
            text += (r % 3 == 0 ? "\r\n" : (r % 3 == 1 ? "\n" : "\r"));
        }
        std::size_t seen = 0;
        const auto found = for_each_csv_record(text, [&](std::string_view raw) {
            std::string single(raw);
            single.push_back('\n');
            std::size_t parsed = 0;
            parser.parse(single, [&](const auto& fields) {
                require(seen < expected.size() && fields == expected[seen],
                        "CSV logical record or fields mismatch");
                ++parsed;
            });
            require(parsed == 1, "CSV record scanner emitted an incomplete record");
            ++seen;
        });
        require(found == expected.size() && seen == expected.size(),
                "CSV record scanner skipped records");
    }
    for (const char* bad : {"\"unterminated", "ab\"cd", "\"a\"z", "\"a\" \n", "\"a\"\""}) {
        expect_throw<std::invalid_argument>([&] {
            for_each_csv_record(bad, [](std::string_view){});
        }, "invalid CSV quoting accepted");
    }
}
} // namespace
int main() {
    try {
        test_dense();
        test_percentiles();
        test_csv_records();
        std::cout << "New independent high-performance primitive tests: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
