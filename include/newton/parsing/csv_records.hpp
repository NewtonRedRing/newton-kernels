#pragma once

#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace newton::parsing {
// Visits raw CSV records without copying or allocating. Quoted fields may
// contain commas, CR, LF, escaped quotes, and CRLF. Each borrowed view excludes
// its ending CR, LF, or CRLF but retains the original bytes of its fields.
// The callback must not retain the view beyond the input's lifetime.
// This is a record BOUNDARY scanner, not a field decoder; callers that need
// unescaped fields should use CsvParser or a dedicated decoding layer.
// Invalid quote placement and unterminated quotes throw invalid_argument.
template<class Callback>
std::size_t for_each_csv_record(std::string_view input, Callback&& callback) {
    enum class State { FieldStart, Unquoted, Quoted, AfterQuoted };
    auto state = State::FieldStart;
    std::size_t start = 0;
    std::size_t pos = 0;
    std::size_t records = 0;

    while (pos < input.size()) {
        const char c = input[pos];
        if (state == State::Quoted) {
            if (c == '"') {
                if (pos + 1 < input.size() && input[pos + 1] == '"') {
                    pos += 2; // Escaped quote: still within the quoted field.
                    continue;
                }
                state = State::AfterQuoted;
            }
            ++pos;
            continue;
        }
        if (c == '\r' || c == '\n') {
            const auto end = pos;
            if (c == '\r' && pos + 1 < input.size() && input[pos + 1] == '\n')
                ++pos;
            ++pos;
            callback(input.substr(start, end - start));
            ++records;
            start = pos;
            state = State::FieldStart;
            continue;
        }
        if (c == ',') {
            state = State::FieldStart;
        } else if (c == '"') {
            if (state == State::FieldStart) state = State::Quoted;
            else throw std::invalid_argument("Invalid quote in CSV field");
        } else {
            if (state == State::AfterQuoted)
                throw std::invalid_argument("Unexpected character after closing CSV quote");
            state = State::Unquoted;
        }
        ++pos;
    }
    if (state == State::Quoted)
        throw std::invalid_argument("Unterminated CSV quoted field");
    if (start < input.size()) {
        callback(input.substr(start));
        ++records;
    }
    return records;
}
} // namespace newton::parsing
