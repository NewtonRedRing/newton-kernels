#include <newton/analytics/csv_fused.hpp>
#include <newton/parsing/csv_parser.hpp>
#include <newton/parsing/ascii_numeric.hpp>
#include <newton/memory/mapped_file.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::string input_data(std::size_t rows) {
    std::string input;
    if (rows > (std::numeric_limits<std::size_t>::max() / 36))
        throw std::length_error("Fixture size too large");
    input.reserve(rows * 36);
    for (std::size_t i = 0; i < rows; ++i) {
        const auto group = (i * 2654435761ULL + 17) % 64;
        const auto measurement = (i * 1315423911ULL + 97) % 1000;
        const auto cents = (i * 337ULL + 43) % 500000;
        const bool quoted = i % 9 == 0;
        if (quoted) input += '"';
        input += std::to_string(group);
        if (quoted) input += '"';
        input += ',';
        input += std::to_string(measurement);
        input += ',';
        input += std::to_string(cents / 100);
        input += '.';
        if (cents % 100 < 10) input += '0';
        input += std::to_string(cents % 100);
        input += (i % 2 == 0 ? "\r\n" : "\n");
    }
    return input;
}
std::uint64_t digest(const std::array<std::uint64_t, 64>& counts,
                     const std::array<std::uint64_t, 64>& sums) {
    std::uint64_t value = 14695981039346656037ULL;
    for (std::size_t i = 0; i < counts.size(); ++i) {
        value = (value ^ counts[i]) * 1099511628211ULL;
        value = (value ^ sums[i]) * 1099511628211ULL;
    }
    return value;
}
struct Aggregate {
    std::array<std::uint64_t, 64> counts{}, sums{};
    std::size_t records = 0, selected = 0;
};
Aggregate run_fused(std::string_view data) {
    Aggregate out;
    std::array<std::string_view, 4> fields{};
    std::array<char, 64> scratch{};
    const auto result = newton::analytics::parse_filter_group_cents_into(
        data, {.filter_greater_than = 500}, out.counts, out.sums, fields, scratch);
    out.records = result.records;
    out.selected = result.selected;
    return out;
}
Aggregate run_owned(std::string_view data) {
    Aggregate out;
    newton::parsing::CsvParser parser;
    const auto records = parser.parse(data, [&](const newton::parsing::CsvRow& fields) {
        if (fields.size() != 3) throw std::invalid_argument("Expected three columns");
        const auto group = newton::parsing::parse_ascii_uint64(fields[0]);
        const auto measurement = newton::parsing::parse_ascii_uint64(fields[1]);
        const auto cents = newton::parsing::parse_decimal_cents(fields[2]);
        if (!group || *group >= out.counts.size() || !measurement ||
            !cents || *cents < 0) throw std::invalid_argument("Invalid CSV row");
        ++out.records;
        if (*measurement <= 500) return;
        const auto index = static_cast<std::size_t>(*group);
        if (out.counts[index] == std::numeric_limits<std::uint64_t>::max() ||
            static_cast<std::uint64_t>(*cents) >
                std::numeric_limits<std::uint64_t>::max() - out.sums[index])
            throw std::overflow_error("Aggregate overflow");
        ++out.counts[index];
        out.sums[index] += static_cast<std::uint64_t>(*cents);
        ++out.selected;
    });
    if (records != out.records) throw std::runtime_error("Record count mismatch");
    return out;
}
} // namespace
int main(int argc, char** argv) {
    try {
        std::size_t rows = 100000;
        std::string mode = "fused";
        std::string input_path, fixture_path;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg.starts_with("--rows=")) rows = std::stoull(std::string(arg.substr(7)));
            else if (arg.starts_with("--impl=")) mode = arg.substr(7);
            else if (arg.starts_with("--input=")) input_path = arg.substr(8);
            else if (arg.starts_with("--save-fixture=")) fixture_path = arg.substr(15);
            else throw std::invalid_argument("Unknown option");
        }
        if (rows > 3000000) throw std::invalid_argument("Limit fixture to 3M rows");
        if (!fixture_path.empty()) {
            const auto fixture = input_data(rows);
            std::ofstream output(fixture_path, std::ios::binary);
            if (!output) throw std::runtime_error("Cannot open fixture file");
            output.write(fixture.data(), static_cast<std::streamsize>(fixture.size()));
            if (!output) throw std::runtime_error("Failed to write complete fixture file");
            return 0;
        }
        std::optional<newton::memory::MappedFile> mapping;
        std::string generated;
        std::string_view data;
        if (!input_path.empty()) {
            mapping.emplace(std::filesystem::path(input_path));
            data = mapping->text();
        } else {
            generated = input_data(rows);
            data = generated;
        }
        Aggregate result;
        if (mode == "fused") result = run_fused(data);
        else if (mode == "owned") result = run_owned(data);
        else throw std::invalid_argument("Unknown implementation");
        std::cout << "{\"checksum\":\"" << digest(result.counts, result.sums)
                  << "\",\"items\":" << result.records
                  << ",\"selected\":" << result.selected << "}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
