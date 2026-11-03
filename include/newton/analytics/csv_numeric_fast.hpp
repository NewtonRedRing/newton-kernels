#pragma once
#include <newton/analytics/csv_fused.hpp>
#include <newton/parsing/ascii_numeric.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>

namespace newton::analytics {

// Single-pass, allocation-free, strict numeric CSV aggregation. All fields
// (including nonnumeric ignored fields) are quote-scanned and validated.
// Selected numeric fields may be quoted, but escaped quotes/newlines cannot
// occur inside a valid integer/decimal numeric token. Row-specific errors
// can leave output partially updated. No hidden state or background workers.
namespace detail {
struct NumericCsvField {
    std::string_view bytes;
    bool end_record = false;
};
inline NumericCsvField next_numeric_csv_field(
        std::string_view csv, std::size_t& cursor) {
    const auto n = csv.size();
    const auto begin = cursor;
    std::size_t start = begin, end = begin;
    if (cursor < n && csv[cursor] == '"') {
        start = ++cursor;
        bool closed = false;
        while (cursor < n) {
            if (csv[cursor] != '"') { ++cursor; continue; }
            if (cursor + 1 < n && csv[cursor + 1] == '"') {
                cursor += 2;
                continue;
            }
            end = cursor++;
            closed = true;
            break;
        }
        if (!closed) throw std::invalid_argument("Unterminated CSV quoted numeric field");
        if (cursor < n && csv[cursor] != ',' &&
            csv[cursor] != '\r' && csv[cursor] != '\n')
            throw std::invalid_argument("Invalid character after closing CSV quote");
    } else {
        start = cursor;
        while (cursor < n && csv[cursor] != ',' &&
               csv[cursor] != '\r' && csv[cursor] != '\n') {
            if (csv[cursor] == '"')
                throw std::invalid_argument("Unexpected CSV quote in unquoted field");
            ++cursor;
        }
        end = cursor;
    }
    const auto field = csv.substr(start, end - start);
    if (cursor == n) return {field, true};
    if (csv[cursor] == ',') {
        ++cursor;
        return {field, false};
    }
    const bool crlf = csv[cursor] == '\r' &&
                       cursor + 1 < n && csv[cursor + 1] == '\n';
    cursor += crlf ? 2 : 1;
    return {field, true};
}
} // namespace detail

// Contract matches parse_filter_group_cents_into for valid numeric CSV;
// the faster entry point does not use field descriptor or decode scratch.
// Non-selected fields may include arbitrary quoted CSV including newlines.
inline CsvAggregateResult parse_filter_group_cents_fast_into(
    std::string_view csv, const CsvAggregateOptions& options,
    std::span<std::uint64_t> counts, std::span<std::uint64_t> sums) {
    if (counts.size() != sums.size())
        throw std::invalid_argument("Fused numeric aggregate output lengths differ");
    for (auto& count : counts) count = 0;
    for (auto& sum : sums) sum = 0;
    CsvAggregateResult result{};
    bool header = options.has_header;
    std::size_t cursor = 0;
    while (cursor < csv.size()) {
        std::uint64_t group = 0, metric = 0;
        std::int64_t cents = 0;
        bool group_ok = false, metric_ok = false, cents_ok = false;
        std::size_t column = 0;
        while (true) {
            const auto field = detail::next_numeric_csv_field(csv, cursor);
            if (!header) {
                if (column == options.group_column) {
                    const auto parsed = newton::parsing::parse_ascii_uint64(field.bytes);
                    if (parsed) { group = *parsed; group_ok = true; }
                }
                if (column == options.filter_column) {
                    const auto parsed = newton::parsing::parse_ascii_uint64(field.bytes);
                    if (parsed) { metric = *parsed; metric_ok = true; }
                }
                if (column == options.value_column) {
                    const auto parsed = newton::parsing::parse_decimal_cents(field.bytes);
                    if (parsed) { cents = *parsed; cents_ok = true; }
                }
            }
            ++column;
            if (field.end_record) break;
        }
        if (header) { header = false; continue; }
        if (!group_ok || group >= counts.size() || !metric_ok || !cents_ok || cents < 0)
            throw std::invalid_argument("Invalid group/filter/amount field");
        ++result.records;
        if (metric <= options.filter_greater_than) continue;
        const auto idx = static_cast<std::size_t>(group);
        const auto amount = static_cast<std::uint64_t>(cents);
        if (counts[idx] == std::numeric_limits<std::uint64_t>::max() ||
            amount > std::numeric_limits<std::uint64_t>::max() - sums[idx])
            throw std::overflow_error("Single-pass numeric aggregate overflow");
        ++counts[idx];
        sums[idx] += amount;
        ++result.selected;
    }
    return result;
}
} // namespace newton::analytics
