#pragma once
#include <cstddef>
#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace newton::parsing {
using CsvRow = std::vector<std::string>;
// Single strict CSV implementation shared by erased and inline callbacks.
// Calling a non-erased callback directly lets the optimizer inline the hot
// consumer (e.g. hashing or aggregation) without std::function dispatch.
// Research experiment: allow the compiler to inline the specialized
// callback parser where its own cost model finds it beneficial. This changes
// no field semantics or callback contract; benchmark code/layout effects.
namespace detail {
template<class Callback>
std::size_t parse_csv_impl(std::string_view input, Callback& on_row) {
    if (input.empty()) return 0;
    CsvRow row;
    row.reserve(8);
    std::size_t records = 0;
    std::size_t cursor = 0;
    const auto size = input.size();
    while (cursor < size) {
        if (input[cursor] == '"') {
            const auto start = ++cursor;
            std::size_t segment = start;
            std::string decoded;
            bool has_escapes = false;
            while (true) {
                const auto quote = input.find('"', cursor);
                if (quote == std::string_view::npos)
                    throw std::invalid_argument("Unterminated CSV quoted field");
                if (quote + 1 < size && input[quote + 1] == '"') {
                    has_escapes = true;
                    decoded.append(input.data() + segment, quote - segment);
                    decoded.push_back('"');
                    cursor = quote + 2;
                    segment = cursor;
                } else {
                    if (has_escapes) {
                        decoded.append(input.data() + segment, quote - segment);
                        row.emplace_back(std::move(decoded));
                    } else {
                        row.emplace_back(input.substr(start, quote - start));
                    }
                    cursor = quote + 1;
                    if (cursor < size && input[cursor] != ',' && input[cursor] != '\r' && input[cursor] != '\n')
                        throw std::invalid_argument("Unexpected character after closing CSV quote");
                    break;
                }
            }
        } else {
            const auto start = cursor;
            while (cursor < size && input[cursor] != ',' && input[cursor] != '\r' &&
                   input[cursor] != '\n' && input[cursor] != '"') ++cursor;
            if (cursor < size && input[cursor] == '"')
                throw std::invalid_argument("Quote in unquoted CSV field");
            row.emplace_back(input.substr(start, cursor - start));
        }
        if (cursor == size) {
            on_row(row);
            ++records;
            break;
        }
        if (input[cursor] == ',') {
            ++cursor;
            if (cursor == size) {
                row.emplace_back();
                on_row(row);
                ++records;
            }
        } else {
            if (input[cursor++] == '\r' && cursor < size && input[cursor] == '\n') ++cursor;
            on_row(row);
            ++records;
            row.clear();
        }
    }
    return records;
}
} // namespace detail

// RFC 4180-style comma-separated records. Embedded newlines, escaped double
// quotes and CRLF are supported; malformed quoting throws std::invalid_argument.
class CsvParser {
public:
    using RowCallback = std::function<void(const CsvRow&)>;
    // Rows are borrowed by the callback. Make a copy if retention is required.
    [[nodiscard]] std::size_t parse(std::string_view input, const RowCallback& on_row) const;
    // Source-compatible, C++20 opt-in for concrete callbacks. Existing
    // std::function callers continue to use the original compiled ABI.
    template<class Callback>
        requires (!std::same_as<std::remove_cvref_t<Callback>, RowCallback> &&
                  std::invocable<Callback&, const CsvRow&>)
    [[nodiscard]] std::size_t parse(std::string_view input, Callback&& on_row) const {
        return detail::parse_csv_impl(input, on_row);
    }
};
} // namespace newton::parsing
