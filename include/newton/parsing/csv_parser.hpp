#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace newton::parsing {
using CsvRow = std::vector<std::string>;
// RFC 4180-style comma-separated records. Embedded newlines, escaped double
// quotes and CRLF are supported; malformed quoting throws std::invalid_argument.
class CsvParser {
public:
    using RowCallback = std::function<void(const CsvRow&)>;
    // Rows are borrowed by the callback. Make a copy if retention is required.
    [[nodiscard]] std::size_t parse(std::string_view input, const RowCallback& on_row) const;
};
} // namespace newton::parsing
