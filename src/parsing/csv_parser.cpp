#include <newton/parsing/csv_parser.hpp>
#include <stdexcept>
#include <utility>

namespace newton::parsing {
std::size_t CsvParser::parse(std::string_view input, const RowCallback& on_row) const {
    if (input.empty()) return 0;
    CsvRow row;
    row.reserve(8);
    // Keep each field's string storage between rows; in fixed-width CSV this
    // avoids repeated destruction/construction (and reuses long-field capacity).
    // Only the logical field count changes at each completed record.
    std::size_t fields = 0;
    const auto append_field = [&](std::string_view value) {
        if (fields < row.size()) row[fields].assign(value);
        else row.emplace_back(value);
        ++fields;
    };
    const auto append_decoded = [&](std::string&& value) {
        if (fields < row.size()) row[fields] = std::move(value);
        else row.emplace_back(std::move(value));
        ++fields;
    };
    std::size_t records = 0;
    const auto emit_row = [&] {
        row.resize(fields);
        on_row(row);
        ++records;
        fields = 0;
    };
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
                        append_decoded(std::move(decoded));
                    } else {
                        append_field(input.substr(start, quote - start));
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
            append_field(input.substr(start, cursor - start));
        }
        if (cursor == size) {
            emit_row();
            break;
        }
        if (input[cursor] == ',') {
            ++cursor;
            if (cursor == size) {
                append_field({});
                emit_row();
            }
        } else {
            if (input[cursor++] == '\r' && cursor < size && input[cursor] == '\n') ++cursor;
            emit_row();
        }
    }
    return records;
}
} // namespace newton::parsing
