#pragma once
#include <newton/parsing/ascii_numeric.hpp>
#include <newton/parsing/csv_fields.hpp>
#include <newton/parsing/csv_records.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>

namespace newton::analytics {

// Input grammar: strict quoted CSV with unsigned dense group IDs, unsigned
// filter measurements, and nonnegative exact decimal values in cents.
// Callers own fields, scratch, group counters/sums. No hidden heap or workers.
// Existing outputs are cleared. Invalid rows can leave outputs partially set.
struct CsvAggregateOptions {
    std::size_t group_column = 0;
    std::size_t filter_column = 1;
    std::size_t value_column = 2;
    std::uint64_t filter_greater_than = 0;
    bool has_header = false;
};
struct CsvAggregateResult {
    std::size_t records = 0;  // excludes an optional header
    std::size_t selected = 0;
};

inline CsvAggregateResult parse_filter_group_cents_into(
    std::string_view csv, const CsvAggregateOptions& options,
    std::span<std::uint64_t> counts,
    std::span<std::uint64_t> sums,
    std::span<std::string_view> fields, std::span<char> decode_scratch) {
    if (counts.size() != sums.size() || fields.empty())
        throw std::invalid_argument("Invalid aggregate output/field capacity");
    for (auto& v : counts) v = 0;
    for (auto& v : sums) v = 0;
    const auto max_col = options.group_column > options.filter_column
        ? (options.group_column > options.value_column ? options.group_column : options.value_column)
        : (options.filter_column > options.value_column ? options.filter_column : options.value_column);
    CsvAggregateResult out{};
    bool skip_header = options.has_header;
    newton::parsing::for_each_csv_record(csv, [&](std::string_view record) {
        if (skip_header) {
            skip_header = false;
            return;
        }
        const auto field_count = newton::parsing::decode_csv_fields_into(
            record, fields, decode_scratch);
        if (max_col >= field_count)
            throw std::invalid_argument("CSV aggregate row has too few fields");
        const auto group = newton::parsing::parse_ascii_uint64(fields[options.group_column]);
        const auto measured = newton::parsing::parse_ascii_uint64(fields[options.filter_column]);
        const auto signed_amount = newton::parsing::parse_decimal_cents(fields[options.value_column]);
        if (!group || *group >= counts.size() || !measured ||
            !signed_amount || *signed_amount < 0)
            throw std::invalid_argument("CSV aggregate row contains an invalid group or number");
        ++out.records;
        if (*measured <= options.filter_greater_than) return;
        const auto idx = static_cast<std::size_t>(*group);
        const auto amount = static_cast<std::uint64_t>(*signed_amount);
        if (counts[idx] == std::numeric_limits<std::uint64_t>::max() ||
            amount > std::numeric_limits<std::uint64_t>::max() - sums[idx])
            throw std::overflow_error("Fused CSV aggregate overflow");
        ++counts[idx];
        sums[idx] += amount;
        ++out.selected;
    });
    return out;
}

} // namespace newton::analytics
