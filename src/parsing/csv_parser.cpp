#include <newton/parsing/csv_parser.hpp>
#include <stdexcept>
#include <utility>

namespace newton::parsing {
std::size_t CsvParser::parse(std::string_view input, const RowCallback& on_row) const {
    if (input.empty()) return 0;
    CsvRow row;
    row.reserve(8);
    std::size_t records = 0;
    std::size_t cursor = 0;
    const auto size = input.size();
    while (cursor < size) {
        // Quote-free physical records can be split with optimized character
        // searches. Any quote before the line ending takes the strict path
        // below, including correctly handling multiline quoted fields.
        const auto eol = input.find_first_of("\r\n", cursor);
        const auto line_end = eol == std::string_view::npos ? size : eol;
        const auto line = input.substr(cursor, line_end - cursor);
        if (line.find('"') == std::string_view::npos) {
            std::size_t field_start = 0;
            for (;;) {
                const auto comma = line.find(',', field_start);
                if (comma == std::string_view::npos) {
                    row.emplace_back(line.substr(field_start));
                    break;
                }
                row.emplace_back(line.substr(field_start, comma - field_start));
                field_start = comma + 1;
            }
            on_row(row);
            ++records;
            row.clear();
            cursor = line_end;
            if (cursor < size) {
                const char end = input[cursor++];
                if (end == '\r' && cursor < size && input[cursor] == '\n')
                    ++cursor;
            }
            continue;
        }
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
} // namespace newton::parsing
