#pragma once
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string_view>

namespace newton::parsing {
// Decode exactly one raw CSV record into non-owning fields. Unescaped fields
// alias the input. Escaped quoted fields are decoded into caller-provided
// scratch; every field view stays valid while both input and scratch live.
// No allocations; malformed quotes and insufficient capacity throw.
inline std::size_t decode_csv_fields_into(
    std::string_view record,
    std::span<std::string_view> fields,
    std::span<char> scratch) {
    std::size_t count = 0, i = 0, used = 0;
    for (;;) {
        if (count == fields.size())
            throw std::length_error("CSV field descriptor capacity exceeded");
        if (i < record.size() && record[i] == '"') {
            ++i;
            const auto field_start = i;
            auto segment = i;
            const auto decoded_start = used;
            bool escaped = false;
            for (;;) {
                if (i == record.size()) throw std::invalid_argument("Unterminated CSV quote");
                if (record[i] != '"') { ++i; continue; }
                if (i + 1 < record.size() && record[i + 1] == '"') {
                    if (used + (i - segment) + 1 > scratch.size())
                        throw std::length_error("CSV decode scratch is too small");
                    for (auto n = segment; n < i; ++n) scratch[used++] = record[n];
                    scratch[used++] = '"';
                    i += 2;
                    segment = i;
                    escaped = true;
                    continue;
                }
                if (escaped) {
                    if (used + (i - segment) > scratch.size())
                        throw std::length_error("CSV decode scratch is too small");
                    for (auto n = segment; n < i; ++n) scratch[used++] = record[n];
                    fields[count++] = std::string_view(scratch.data() + decoded_start, used - decoded_start);
                } else {
                    fields[count++] = record.substr(field_start, i - field_start);
                }
                ++i;
                if (i < record.size() && record[i] != ',')
                    throw std::invalid_argument("Invalid character after CSV quoted field");
                break;
            }
        } else {
            const auto start = i;
            while (i < record.size() && record[i] != ',') {
                if (record[i] == '"') throw std::invalid_argument("Quote inside unquoted field");
                ++i;
            }
            fields[count++] = record.substr(start, i - start);
        }
        if (i == record.size()) return count;
        ++i; // comma
        if (i == record.size()) {
            if (count == fields.size()) throw std::length_error("CSV trailing field needs capacity");
            fields[count++] = record.substr(i, 0);
            return count;
        }
    }
}
} // namespace newton::parsing
